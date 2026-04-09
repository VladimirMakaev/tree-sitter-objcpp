#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <assert.h>
#include <string.h>
#include <wctype.h>

enum TokenType {
    RAW_STRING_DELIMITER,
    RAW_STRING_CONTENT,
    ATTR_OPEN,
    TYPE_TRAIT_KEYWORD,
    SELECTOR_NAME_KEYWORD,
    OBJC_TYPE_QUALIFIER_KEYWORD,
    OBJC_STORAGE_CLASS_KEYWORD,
};

/// The spec limits delimiters to 16 chars
#define MAX_DELIMITER_LENGTH 16

typedef struct {
    uint8_t delimiter_length;
    wchar_t delimiter[MAX_DELIMITER_LENGTH];
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip_ws(TSLexer *lexer) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
           lexer->lookahead == '\n' || lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
    }
}

static inline void reset(Scanner *scanner) {
    scanner->delimiter_length = 0;
    memset(scanner->delimiter, 0, sizeof scanner->delimiter);
}

/// Scan the raw string delimiter in R"delimiter(content)delimiter"
static bool scan_raw_string_delimiter(Scanner *scanner, TSLexer *lexer) {
    if (scanner->delimiter_length > 0) {
        // Closing delimiter: must exactly match the opening delimiter.
        // We already checked this when scanning content, but this is how we
        // know when to stop. We can't stop at ", because R"""hello""" is valid.
        for (int i = 0; i < scanner->delimiter_length; ++i) {
            if (lexer->lookahead != scanner->delimiter[i]) {
                return false;
            }
            advance(lexer);
        }
        reset(scanner);
        return true;
    }

    // Opening delimiter: record the d-char-sequence up to (.
    // d-char is any basic character except parens, backslashes, and spaces.
    for (;;) {
        if (scanner->delimiter_length >= MAX_DELIMITER_LENGTH || lexer->eof(lexer) || lexer->lookahead == '\\' ||
            iswspace(lexer->lookahead)) {
            return false;
        }
        if (lexer->lookahead == '(') {
            // Rather than create a token for an empty delimiter, we fail and
            // let the grammar fall back to a delimiter-less rule.
            return scanner->delimiter_length > 0;
        }
        scanner->delimiter[scanner->delimiter_length++] = lexer->lookahead;
        advance(lexer);
    }
}

/// Scan the raw string content in R"delimiter(content)delimiter"
static bool scan_raw_string_content(Scanner *scanner, TSLexer *lexer) {
    // The progress made through the delimiter since the last ')'.
    // The delimiter may not contain ')' so a single counter suffices.
    for (int delimiter_index = -1;;) {
        // If we hit EOF, consider the content to terminate there.
        // This forms an incomplete raw_string_literal, and models the code
        // well.
        if (lexer->eof(lexer)) {
            lexer->mark_end(lexer);
            return true;
        }

        if (delimiter_index >= 0) {
            if (delimiter_index == scanner->delimiter_length) {
                if (lexer->lookahead == '"') {
                    return true;
                }
                delimiter_index = -1;
            } else {
                if (lexer->lookahead == scanner->delimiter[delimiter_index]) {
                    delimiter_index += 1;
                } else {
                    delimiter_index = -1;
                }
            }
        }

        if (delimiter_index == -1 && lexer->lookahead == ')') {
            // The content doesn't include the )delimiter" part.
            // We must still scan through it, but exclude it from the token.
            lexer->mark_end(lexer);
            delimiter_index = 0;
        }

        advance(lexer);
    }
}

/// Peek at [[ to determine if it's a C++ attribute or an ObjC nested message.
/// Returns true (producing ATTR_OPEN token) if it's an attribute [[.
/// Returns false if it's a nested message send [[Foo alloc] init].
static bool scan_attr_open(TSLexer *lexer) {
    // Must see first [
    if (lexer->lookahead != '[') return false;
    advance(lexer);

    // Must see second [
    if (lexer->lookahead != '[') return false;
    advance(lexer);

    // Mark end here — the token is [[
    lexer->mark_end(lexer);

    // Skip whitespace after [[
    skip_ws(lexer);

    // EOF after [[ — not an attribute
    if (lexer->eof(lexer)) return false;

    // Special case: [[using — always an attribute
    // Check for 'using' followed by whitespace or identifier char
    if (lexer->lookahead == 'u') {
        const char *using_rest = "sing";
        int32_t saved = lexer->lookahead;
        advance(lexer);
        for (int i = 0; using_rest[i]; i++) {
            if (lexer->lookahead != using_rest[i]) goto not_using;
            advance(lexer);
        }
        // After "using", check for whitespace or non-identifier char
        if (!iswalnum(lexer->lookahead) && lexer->lookahead != '_') {
            return true; // [[using ...]] attribute
        }
    not_using:;
        // Fall through — it started with 'u' but wasn't "using"
        // We've consumed chars but that's fine, we already marked end at [[
        // Need to re-check: read rest of identifier
    }

    // Read identifier (we may have partially consumed it in the 'using' check,
    // but for the purpose of disambiguation, we just need to find what follows
    // the first identifier after [[)

    // If current char is not an identifier start, it might be a special char
    // For attributes: [[, ]], could have ( for [[deprecated("msg")]]
    // Non-identifier starts: might be ] or other punct
    if (lexer->lookahead == ']') {
        // [[] — likely error, not attribute
        return false;
    }

    // Re-scan: skip to end of first identifier after [[
    // We need a fresh approach: re-skip whitespace and read full identifier
    // But we can't rewind the lexer. The 'using' path may have consumed chars.
    // Since we marked end at [[, let's just scan forward to find the deciding character.

    // Skip any remaining identifier chars (from partial 'using' consumption or fresh)
    while (iswalnum(lexer->lookahead) || lexer->lookahead == '_') {
        advance(lexer);
    }

    // Skip whitespace after identifier
    skip_ws(lexer);

    if (lexer->eof(lexer)) return false;

    // Decision point: what follows the identifier?
    switch (lexer->lookahead) {
        case ']':  // [[identifier] — attribute (e.g., [[deprecated]])
        case ',':  // [[identifier, — attribute (e.g., [[deprecated, nodiscard]])
        case '(':  // [[identifier( — attribute with args (e.g., [[deprecated("msg")]])
            return true;
        case ':':  // Could be [[namespace::attr]] or [[Foo alloc] method:]
            advance(lexer);
            if (lexer->lookahead == ':') {
                return true;  // [[ns::attr]] — scoped attribute
            }
            return false;  // [[Foo method:] — message send with keyword
        default:
            // Another identifier follows — this is [[Receiver method]
            // e.g., [[Foo alloc] init]
            return false;
    }
}

/// Check if a string matches a known type trait prefix after "__"
/// Known prefixes: is_, has_, add_, remove_, make_, reference_, decay, underlying_type
static bool is_type_trait_prefix(const char *buf, int len) {
    if (len >= 3 && buf[0] == 'i' && buf[1] == 's' && buf[2] == '_') return true;
    if (len >= 4 && buf[0] == 'h' && buf[1] == 'a' && buf[2] == 's' && buf[3] == '_') return true;
    if (len >= 4 && buf[0] == 'a' && buf[1] == 'd' && buf[2] == 'd' && buf[3] == '_') return true;
    if (len >= 7 && memcmp(buf, "remove_", 7) == 0) return true;
    if (len >= 5 && memcmp(buf, "make_", 5) == 0) return true;
    if (len >= 10 && memcmp(buf, "reference_", 10) == 0) return true;
    if (len == 5 && memcmp(buf, "decay", 5) == 0) return true;
    if (len == 16 && memcmp(buf, "underlying_type", 15) == 0) return true;
    return false;
}

/// Helper: check if a string is in a null-terminated keyword list
static bool in_keyword_list(const char *word, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcmp(word, list[i]) == 0) return true;
    }
    return false;
}

void *tree_sitter_objcpp_external_scanner_create() {
    Scanner *scanner = (Scanner *)ts_calloc(1, sizeof(Scanner));
    memset(scanner, 0, sizeof(Scanner));
    return scanner;
}

bool tree_sitter_objcpp_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;

    // Error recovery: if all externals are valid, bail out
    if (valid_symbols[RAW_STRING_DELIMITER] && valid_symbols[RAW_STRING_CONTENT] &&
        valid_symbols[ATTR_OPEN] && valid_symbols[TYPE_TRAIT_KEYWORD] &&
        valid_symbols[SELECTOR_NAME_KEYWORD] && valid_symbols[OBJC_TYPE_QUALIFIER_KEYWORD] &&
        valid_symbols[OBJC_STORAGE_CLASS_KEYWORD]) {
        return false;
    }

    // Try keyword-based external tokens when raw string tokens are NOT valid
    if (!valid_symbols[RAW_STRING_DELIMITER] && !valid_symbols[RAW_STRING_CONTENT]) {

        // Try ATTR_OPEN first — it matches '[' so won't interfere with keywords
        if (valid_symbols[ATTR_OPEN]) {
            // Skip leading whitespace before checking for [[
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                   lexer->lookahead == '\n' || lexer->lookahead == '\r') {
                lexer->advance(lexer, true);
            }
            if (lexer->lookahead == '[') {
                lexer->result_symbol = ATTR_OPEN;
                if (scan_attr_open(lexer)) {
                    return true;
                }
                // Not an attribute [[ — fall through to try keywords or return false
            }
        }

        bool want_any_keyword = valid_symbols[TYPE_TRAIT_KEYWORD] ||
            valid_symbols[OBJC_TYPE_QUALIFIER_KEYWORD] ||
            valid_symbols[OBJC_STORAGE_CLASS_KEYWORD] ||
            valid_symbols[SELECTOR_NAME_KEYWORD];

        if (want_any_keyword) {
            // Skip leading whitespace (may already be skipped by ATTR_OPEN path)
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                   lexer->lookahead == '\n' || lexer->lookahead == '\r') {
                lexer->advance(lexer, true);
            }

            // Only try if we see an identifier-start character
            if (iswalpha(lexer->lookahead) || lexer->lookahead == '_') {
                // Read the full identifier into a buffer (once)
                char buf[64];
                int len = 0;
                while (iswalnum(lexer->lookahead) || lexer->lookahead == '_') {
                    if (len < 63) buf[len] = (char)lexer->lookahead;
                    len++;
                    advance(lexer);
                }
                if (len > 0 && len < 63) {
                    buf[len] = '\0';

                    // Must not be followed by more identifier chars (already ensured by loop exit)
                    lexer->mark_end(lexer);

                    // Check type trait keywords: __is_*, __has_*, __add_*, __remove_*, __make_*, __decay, __underlying_type, __reference_*
                    if (valid_symbols[TYPE_TRAIT_KEYWORD] && buf[0] == '_' && buf[1] == '_' && len > 2) {
                        if (is_type_trait_prefix(buf + 2, len - 2)) {
                            // Type traits must be followed by '('
                            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                                   lexer->lookahead == '\n' || lexer->lookahead == '\r') {
                                advance(lexer);
                            }
                            if (lexer->lookahead == '(') {
                                lexer->result_symbol = TYPE_TRAIT_KEYWORD;
                                return true;
                            }
                        }
                    }

                    // Check ObjC type qualifier keywords
                    if (valid_symbols[OBJC_TYPE_QUALIFIER_KEYWORD]) {
                        static const char *tq_keywords[] = {
                            "nonnull", "nullable",
                            "_Complex",
                            "_Nonnull", "_Nullable", "_Nullable_result", "_Null_unspecified",
                            "__autoreleasing", "__block",
                            "__bridge", "__bridge_retained", "__bridge_transfer",
                            "__complex", "__const",
                            "__imag", "__kindof",
                            "__nonnull", "__nullable",
                            "__ptrauth_objc_class_ro", "__ptrauth_objc_isa_pointer",
                            "__ptrauth_objc_super_pointer",
                            "__real", "__strong",
                            "__unsafe_unretained", "__unused", "__weak",
                            NULL,
                        };
                        if (in_keyword_list(buf, tq_keywords)) {
                            lexer->result_symbol = OBJC_TYPE_QUALIFIER_KEYWORD;
                            return true;
                        }
                    }

                    // Check ObjC storage class keywords
                    if (valid_symbols[OBJC_STORAGE_CLASS_KEYWORD]) {
                        static const char *sc_keywords[] = {
                            "__inline__",
                            "CG_EXTERN", "CG_INLINE",
                            "FOUNDATION_EXPORT", "FOUNDATION_EXTERN", "FOUNDATION_STATIC_INLINE",
                            "IBOutlet", "IBInspectable", "IB_DESIGNABLE",
                            "NS_INLINE", "NS_VALID_UNTIL_END_OF_SCOPE",
                            "OBJC_EXPORT", "OBJC_ROOT_CLASS",
                            "UIKIT_EXTERN",
                            NULL,
                        };
                        if (in_keyword_list(buf, sc_keywords)) {
                            lexer->result_symbol = OBJC_STORAGE_CLASS_KEYWORD;
                            return true;
                        }
                    }

                    // Check selector name keywords
                    if (valid_symbols[SELECTOR_NAME_KEYWORD]) {
                        static const char *sel_keywords[] = {
                            "new", "delete",
                            "and", "and_eq", "bitand", "bitor", "compl",
                            "not", "not_eq", "or", "or_eq", "xor", "xor_eq",
                            "noexcept", "nullptr",
                            "private", "protected", "public",
                            "this",
                            NULL,
                        };
                        if (in_keyword_list(buf, sel_keywords)) {
                            lexer->result_symbol = SELECTOR_NAME_KEYWORD;
                            return true;
                        }
                    }
                }
            }
        }
    }

    // No skipping leading whitespace: raw-string grammar is space-sensitive.
    if (valid_symbols[RAW_STRING_DELIMITER]) {
        lexer->result_symbol = RAW_STRING_DELIMITER;
        return scan_raw_string_delimiter(scanner, lexer);
    }

    if (valid_symbols[RAW_STRING_CONTENT]) {
        lexer->result_symbol = RAW_STRING_CONTENT;
        return scan_raw_string_content(scanner, lexer);
    }

    return false;
}

unsigned tree_sitter_objcpp_external_scanner_serialize(void *payload, char *buffer) {
    static_assert(MAX_DELIMITER_LENGTH * sizeof(wchar_t) < TREE_SITTER_SERIALIZATION_BUFFER_SIZE,
                  "Serialized delimiter is too long!");

    Scanner *scanner = (Scanner *)payload;
    size_t size = scanner->delimiter_length * sizeof(wchar_t);
    memcpy(buffer, scanner->delimiter, size);
    return (unsigned)size;
}

void tree_sitter_objcpp_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    assert(length % sizeof(wchar_t) == 0 && "Can't decode serialized delimiter!");

    Scanner *scanner = (Scanner *)payload;
    scanner->delimiter_length = length / sizeof(wchar_t);
    if (length > 0) {
        memcpy(&scanner->delimiter[0], buffer, length);
    }
}

void tree_sitter_objcpp_external_scanner_destroy(void *payload) {
    Scanner *scanner = (Scanner *)payload;
    ts_free(scanner);
}
