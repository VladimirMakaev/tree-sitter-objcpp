#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <assert.h>
#include <string.h>
#include <wctype.h>

enum TokenType { RAW_STRING_DELIMITER, RAW_STRING_CONTENT, ATTR_OPEN };

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

void *tree_sitter_objcpp_external_scanner_create() {
    Scanner *scanner = (Scanner *)ts_calloc(1, sizeof(Scanner));
    memset(scanner, 0, sizeof(Scanner));
    return scanner;
}

bool tree_sitter_objcpp_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;

    // Error recovery: if all externals are valid, bail out
    if (valid_symbols[RAW_STRING_DELIMITER] && valid_symbols[RAW_STRING_CONTENT] && valid_symbols[ATTR_OPEN]) {
        return false;
    }

    // Try ATTR_OPEN when valid and raw string tokens are NOT valid
    if (valid_symbols[ATTR_OPEN] && !valid_symbols[RAW_STRING_DELIMITER] && !valid_symbols[RAW_STRING_CONTENT]) {
        // Skip leading whitespace before checking for [[
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
               lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            lexer->advance(lexer, true);  // skip = true for leading whitespace
        }
        lexer->result_symbol = ATTR_OPEN;
        if (scan_attr_open(lexer)) {
            return true;
        }
        // Not an attribute [[ — return false so parser can try message_expression
        return false;
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
