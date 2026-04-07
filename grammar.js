/**
 * @file Objective-C++ grammar for tree-sitter
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const CPP = require('tree-sitter-cpp/grammar');
const C = require('tree-sitter-c/grammar');

const PREC = Object.assign(C.PREC, {
  LAMBDA: 18,
  NEW: C.PREC.CALL + 1,
  STRUCTURED_BINDING: -1,
  THREE_WAY: C.PREC.RELATIONAL + 1,
});

module.exports = grammar(CPP, {
  name: 'objcpp',

  externals: ($, original) => original.concat([$._attr_open]),

  conflicts: ($, original) => original.concat([
    [$.parameterized_arguments],
    [$._declarator, $.type_specifier, $.sized_type_specifier],
    [$._declarator, $.type_specifier, $.template_type, $.template_function, $.generic_specifier],
    [$.template_function, $.template_type, $.expression, $.generic_specifier],
    [$._declarator, $.type_specifier, $.expression, $.template_type, $.template_function, $.generic_specifier],
    [$._declarator, $.expression, $.template_type, $.template_function],
    [$._declaration_specifiers, $._constructor_specifiers, $._class_interface_header, $._class_implementation_header, $.protocol_declaration, $.protocol_forward_declaration],
    [$.type_descriptor, $.specifier_qualifier],
    [$._declaration_modifiers, $.property_declaration],
    [$._declaration_modifiers, $.specifier_qualifier],
    [$._declaration_specifiers, $.specifier_qualifier],
    [$.destructor_name, $._scope_resolution],
    [$._declarator, $.template_type, $.template_function],
    [$.template_type, $.template_function, $.qualified_identifier, $.qualified_type_identifier],
    [$.keyword_declarator, $.method_parameter],
    [$.attributed_declarator, $.struct_declarator],
    [$._function_attributes_start, $._function_attributes_end],
    [$.expression, $._class_name],
    [$.template_type, $.template_method],
    [$.string_literal],
    [$.specifier_qualifier, $.block_pointer_declarator],
    [$.type_name, $.block_pointer_declarator],
  ]),

  inline: ($, original) => original.concat([
    $.method_selector,
    $.method_selector_no_list,
    $.keyword_selector,
    $.interface_declaration,
    $.typedefed_identifier,
    $.keyword_identifier,
  ]),

  supertypes: ($, original) => original.concat([
    $.specifier_qualifier,
  ]),

  rules: {
    // Override attribute_declaration to use external scanner for [[ disambiguation
    attribute_declaration: $ => seq($._attr_open, commaSep1($.attribute), ']]'),

    _top_level_item: ($, original) => choice(
      original,
      $.class_interface,
      $.class_implementation,
      $.protocol_declaration,
      $.class_forward_declaration,
      $.protocol_forward_declaration,
      $.module_import,
      $.compatibility_alias_declaration,
    ),

    _block_item: ($, original) => choice(
      original,
      $.class_interface,
      $.class_implementation,
      $.protocol_declaration,
      $.class_forward_declaration,
      $.protocol_forward_declaration,
      $.module_import,
      $.compatibility_alias_declaration,
    ),

    // --- ObjC class forward declarations ---

    class_forward_declaration: $ => seq(
      '@class',
      commaSep1(seq($.identifier, optional($.parameterized_arguments))),
      ';',
    ),

    // --- ObjC @interface ---

    class_interface: $ => seq(
      $._class_interface_header,
      optional($._type_params),
      optional($._class_interface_inheritance),
      optional($.parameterized_arguments),
      optional($.instance_variables),
      repeat($.interface_declaration),
      '@end',
    ),

    _class_interface_header: $ => seq(
      repeat($._declaration_modifiers),
      '@interface',
      $.identifier,
    ),

    _type_params: $ => prec.right(choice(
      seq($.generic_arguments, optional($.parameterized_arguments)),
      $.parameterized_arguments,
    )),

    _class_interface_inheritance: $ => prec.right(1, choice(
      seq(':', field('superclass', $.identifier), optional($.parameterized_arguments)),
      seq('(', field('category', optional($.identifier)), ')'),
    )),

    // --- ObjC @implementation ---

    class_implementation: $ => seq(
      $._class_implementation_header,
      optional($._type_params),
      optional($._class_implementation_inheritance),
      optional($.instance_variables),
      repeat($.implementation_definition),
      '@end',
    ),

    _class_implementation_header: $ => seq(
      repeat($._declaration_modifiers),
      '@implementation',
      $.identifier,
    ),

    _class_implementation_inheritance: $ => prec(1, choice(
      seq(':', field('superclass', $.identifier)),
      seq('(', field('category', $.identifier), ')'),
    )),

    // --- ObjC @protocol ---

    protocol_declaration: $ => seq(
      repeat($._declaration_modifiers),
      '@protocol',
      $.identifier,
      optional($.protocol_reference_list),
      repeat($.interface_declaration),
      repeat($.qualified_protocol_interface_declaration),
      '@end',
    ),

    protocol_forward_declaration: $ => seq(
      repeat($._declaration_modifiers),
      '@protocol',
      commaSep1($.identifier),
      ';',
    ),

    protocol_reference_list: $ => seq('<', commaSep1($.identifier), '>'),

    // --- Generics / parameterized types ---

    parameterized_arguments: $ => prec(-1, seq(
      '<',
      choice(
        commaSep1(seq(
          commaSep1(seq(optional(choice('__covariant', '__contravariant')), $._type_identifier)),
          optional(seq(':', $.type_name)),
        )),
        commaSep1($.type_name),
      ),
      '>',
    )),

    generic_arguments: $ => prec.right(seq('(', commaSep1($._type_identifier), ')')),

    // --- Instance variables ---

    instance_variables: $ => seq(
      '{',
      repeat(seq(
        optional(choice($.attribute_specifier, $.attribute_declaration)),
        $.instance_variable,
      )),
      '}',
      optional(';'),
    ),

    instance_variable: $ => choice(
      $.visibility_specification,
      $.struct_declaration,
      $.preproc_ifdef,
      $.preproc_if,
    ),

    visibility_specification: _ => choice(
      '@private',
      '@protected',
      '@package',
      '@public',
    ),

    // --- Interface declarations (inside @interface/@protocol) ---

    interface_declaration: $ => choice(
      $.declaration,
      $.property_declaration,
      $.method_declaration,
      $.function_definition,
      $.type_definition,
      $.static_assert_declaration,
      $.template_declaration,
      $.alias_declaration,
      $.using_declaration,
      alias($.preproc_if_in_interface_declaration, $.preproc_if),
      alias($.preproc_ifdef_in_interface_declaration, $.preproc_ifdef),
      $.preproc_def,
      $.preproc_call,
      seq($.struct_specifier, ';'),
    ),

    qualified_protocol_interface_declaration: $ => choice(
      seq('@optional', repeat($.interface_declaration)),
      seq('@required', repeat($.interface_declaration)),
    ),

    // --- Implementation definitions (inside @implementation) ---

    implementation_definition: $ => prec(1, choice(
      $.function_definition,
      $.declaration,
      $.property_implementation,
      seq($.struct_specifier, ';'),
      $.method_definition,
      $.preproc_function_def,
      $.macro_type_specifier,
      $.type_definition,
      $.static_assert_declaration,
      $.template_declaration,
      $.alias_declaration,
      $.using_declaration,
      alias($.preproc_if_in_implementation_definition, $.preproc_if),
      alias($.preproc_ifdef_in_implementation_definition, $.preproc_ifdef),
      $.preproc_def,
      $.preproc_call,
    )),

    property_implementation: $ => choice(
      seq(
        '@synthesize',
        commaSep1(seq($.identifier, optional(seq('=', $.identifier)))),
        ';',
      ),
      seq('@dynamic', optional('(class)'), commaSep1($.identifier), ';'),
    ),

    // --- Method declarations and definitions ---

    method_definition: $ => seq(
      choice('+', '-'),
      optional($.method_type),
      optional($.attribute_specifier),
      choice(
        seq(
          $.method_selector_no_list,
          optional(seq(
            $.method_parameter,
            repeat(seq(optional($.method_selector), $.method_parameter)),
          )),
        ),
        seq(
          $.method_parameter,
          repeat(seq(optional($.method_selector), $.method_parameter)),
        ),
      ),
      optional(seq(',', choice('...', commaSep1(alias($.c_method_parameter, $.method_parameter))))),
      repeat($.declaration),
      repeat($._declaration_modifiers),
      optional(';'),
      $.compound_statement,
      optional(';'),
    ),

    method_type: $ => seq(
      '(',
      commaSep1(seq(optional($.attribute_specifier), choice($.type_name, $.parameterized_arguments))),
      ')',
    ),

    method_selector: $ => prec.left(choice(
      $.method_selector_no_list,
      seq($.keyword_selector, ','),
    )),

    method_selector_no_list: $ => choice(
      $.identifier,
      $.keyword_selector,
      seq($.keyword_selector, ',', '...'),
    ),

    keyword_selector: $ => repeat1($.keyword_declarator),

    keyword_declarator: $ => seq(
      optional($.identifier),
      ':',
      optional($.method_type),
      $.identifier,
    ),

    method_declaration: $ => seq(
      choice('+', '-'),
      optional($.method_type),
      optional(choice($.attribute_specifier, $.attribute_declaration)),
      repeat1(choice(
        seq(
          $.method_selector,
          optional($.attribute_specifier),
          optional($.method_parameter),
        ),
        $.method_parameter,
      )),
      optional(seq(',', choice('...', commaSep1(alias($.c_method_parameter, $.method_parameter))))),
      repeat($._declaration_modifiers),
      repeat1(prec.right(';')),
    ),

    method_parameter: $ => prec.right(seq(
      ':',
      optional($.method_type),
      optional($._declaration_modifiers),
      choice($.identifier, $.keyword_identifier),
      repeat($._declaration_modifiers),
    )),

    c_method_parameter: $ => prec.left(seq(
      $._declaration_specifiers,
      commaSep1(prec.right(field('declarator', choice(
        $._declarator,
        $.init_declarator,
      )))),
    )),

    // --- Properties ---

    property_declaration: $ => seq(
      optional($._declaration_modifiers),
      '@property',
      optional($.property_attributes_declaration),
      optional(choice($.attribute_specifier, $.attribute_declaration)),
      choice($.struct_declaration, $.declaration),
    ),

    property_attributes_declaration: $ => seq(
      '(',
      commaSep($.property_attribute),
      ')',
    ),

    property_attribute: $ => choice(
      $.identifier,
      seq($.identifier, '=', $.identifier, optional(':')),
    ),

    // --- ObjC type system additions ---

    type_name: $ => prec.right(seq(
      repeat1(prec.right(choice($.specifier_qualifier, $.attribute_specifier, $._declarator))),
      optional($.protocol_reference_list),
      optional($._abstract_declarator),
    )),

    specifier_qualifier: $ => prec.right(choice(
      $.type_specifier,
      $.type_qualifier,
      $.protocol_qualifier,
    )),

    protocol_qualifier: _ => choice(
      'out',
      'inout',
      'bycopy',
      'byref',
      'oneway',
      'in',
    ),

    typedefed_identifier: _ => choice(
      'BOOL',
      'IMP',
      'SEL',
      'Class',
      'id',
    ),

    typedefed_specifier: $ => prec.right(seq(
      $.typedefed_identifier,
      optional($.protocol_reference_list),
    )),

    // Extend C++ type_specifier with ObjC additions
    type_specifier: ($, original) => choice(
      original,
      $.typedefed_specifier,
      $.generic_specifier,
      $.typeof_specifier,
    ),

    typeof_specifier: $ => seq(
      choice('__typeof__', '__typeof', 'typeof'),
      '(',
      choice($.expression, $.type_descriptor),
      ')',
    ),

    generic_specifier: $ => prec.right(seq(
      $._type_identifier,
      repeat1(seq(
        '<',
        commaSep1($.type_name),
        '>',
      )),
    )),

    // --- Struct declarations (used in properties and ivars) ---

    struct_declaration: $ => seq(
      repeat1($.specifier_qualifier),
      commaSep1($.struct_declarator),
      optional($._declaration_modifiers),
      ';',
    ),

    struct_declarator: $ => choice(
      $._declarator,
      seq(optional($._declarator), ':', $.expression),
    ),

    // --- Method identifiers ---

    method_identifier: $ => prec.right(seq(
      optional($.identifier),
      repeat1(token.immediate(':')),
      repeat(seq(
        $.identifier,
        repeat1(token.immediate(':')),
      )),
    )),

    keyword_identifier: $ => alias(
      prec(-3,
        choice(
          'id',
          'in',
        ),
      ),
      $.identifier,
    ),

    // --- Module import ---

    module_import: $ => seq(
      '@import',
      field('path', seq($.identifier, repeat(seq('.', $.identifier)))),
      ';',
    ),

    // --- Compatibility alias ---

    compatibility_alias_declaration: $ => seq(
      '@compatibility_alias',
      field('class', $.identifier),
      field('alias', $.identifier),
      ';',
    ),

    // --- Availability attribute specifier ---

    availability_attribute_specifier: $ => choice(
      'NS_AUTOMATED_REFCOUNT_UNAVAILABLE',
      'NS_ROOT_CLASS',
      'NS_UNAVAILABLE',
      'NS_REQUIRES_NIL_TERMINATION',
      'CF_RETURNS_RETAINED',
      'CF_RETURNS_NOT_RETAINED',
      'DEPRECATED_ATTRIBUTE',
      'UI_APPEARANCE_SELECTOR',
      'UNAVAILABLE_ATTRIBUTE',
      seq(
        choice(
          'CF_FORMAT_FUNCTION',
          'NS_AVAILABLE',
          '__IOS_AVAILABLE',
          'NS_AVAILABLE_IOS',
          'API_AVAILABLE',
          'API_UNAVAILABLE',
          'API_DEPRECATED',
          'NS_ENUM_AVAILABLE_IOS',
          'NS_DEPRECATED_IOS',
          'NS_ENUM_DEPRECATED_IOS',
          'NS_FORMAT_FUNCTION',
          'DEPRECATED_MSG_ATTRIBUTE',
          '__deprecated_msg',
          '__deprecated_enum_msg',
          'NS_SWIFT_NAME',
          'NS_SWIFT_UNAVAILABLE',
          'NS_EXTENSION_UNAVAILABLE_IOS',
          'NS_CLASS_AVAILABLE_IOS',
          'NS_CLASS_DEPRECATED_IOS',
          '__OSX_AVAILABLE_STARTING',
        ),
        '(',
        commaSep1(choice(
          $.string_literal,
          $.concatenated_string,
          $.version,
          $.method_identifier,
          $.identifier,
          seq($.identifier, '(', optional($.method_identifier), ')'),
        )),
        ')',
      ),
    ),

    _declaration_modifiers: ($, original) => choice(
      original,
      $.availability_attribute_specifier,
    ),

    // --- Version / availability ---

    availability: $ => seq(
      'availability',
      '(',
      commaSep1(seq(
        $.identifier,
        optional(seq(
          '=',
          choice($.version, $.expression),
        )),
      )),
      ')',
    ),

    version: $ => prec.right(choice(
      $.platform,
      $.version_number,
      seq(
        $.platform,
        '(',
        commaSep1(choice($.number_literal, $.identifier)),
        ')',
      ),
    )),

    version_number: _ => /\d+([\._]\d+)*/,

    platform: _ => choice('ios', 'tvos', 'macos', 'macosx', 'watchos'),

    // --- Preprocessor extensions ---

    ...preprocIf('_in_implementation_definition', $ => $.implementation_definition),
    ...preprocIf('_in_interface_declaration', $ => $.interface_declaration),

    // --- ObjC Statements ---

    _non_case_statement: ($, original) => choice(
      original,
      $.objc_try_statement,
      $.objc_throw_statement,
      $.synchronized_statement,
    ),

    objc_try_statement: $ => seq(
      '@try',
      $.compound_statement,
      choice(
        seq(repeat1($.objc_catch_clause), optional($.objc_finally_clause)),
        $.objc_finally_clause,
      ),
    ),

    objc_catch_clause: $ => seq(
      '@catch',
      optional(seq('(', choice('...', $.type_name), ')')),
      $.compound_statement,
    ),

    objc_finally_clause: $ => seq(
      '@finally',
      $.compound_statement,
    ),

    objc_throw_statement: $ => seq(
      '@throw',
      optional($.expression),
      ';',
    ),

    synchronized_statement: $ => seq(
      '@synchronized',
      '(',
      commaSep1($.expression),
      ')',
      $.compound_statement,
    ),

    // Override compound_statement to support @autoreleasepool prefix
    compound_statement: ($, original) => prec(-1, seq(
      optional('@autoreleasepool'),
      original,
    )),

    // Override for_statement to support for-in (fast enumeration)
    for_statement: ($, original) => choice(
      original,
      prec(1, seq(
        'for',
        '(',
        choice(
          seq($._declaration_specifiers, $._declarator),
          $.identifier,
        ),
        'in',
        $.expression,
        ')',
        $._non_case_statement,
      )),
    ),

    // --- Block pointer declarators ---

    _declarator: ($, original) => choice(
      original,
      $.block_pointer_declarator,
    ),

    _abstract_declarator: ($, original) => choice(
      original,
      $.abstract_block_pointer_declarator,
    ),

    _field_declarator: ($, original) => choice(
      original,
      alias($.block_pointer_field_declarator, $.block_pointer_declarator),
    ),

    _type_declarator: ($, original) => choice(
      original,
      alias($.block_pointer_type_declarator, $.block_pointer_declarator),
    ),

    block_pointer_declarator: $ => prec.dynamic(1, prec.right(seq(
      '^',
      repeat($.type_qualifier),
      field('declarator', $._declarator),
    ))),
    block_pointer_field_declarator: $ => prec.dynamic(1, prec.right(seq(
      '^',
      repeat($.type_qualifier),
      field('declarator', $._field_declarator),
    ))),
    block_pointer_type_declarator: $ => prec.dynamic(1, prec.right(seq(
      '^',
      repeat($.type_qualifier),
      field('declarator', $._type_declarator),
    ))),
    abstract_block_pointer_declarator: $ => prec.dynamic(1, prec.right(seq(
      '^',
      repeat($.type_qualifier),
      field('declarator', optional($._abstract_declarator)),
    ))),

    // --- Type qualifiers (ARC, nullability, etc.) ---

    type_qualifier: (_, original) => prec.right(choice(
      original,
      'nullable',
      '_Complex',
      '_Nonnull',
      '_Nullable',
      '_Nullable_result',
      '_Null_unspecified',
      '__autoreleasing',
      '__block',
      '__bridge',
      '__bridge_retained',
      '__bridge_transfer',
      '__complex',
      '__const',
      '__imag',
      '__kindof',
      '__nonnull',
      '__nullable',
      '__ptrauth_objc_class_ro',
      '__ptrauth_objc_isa_pointer',
      '__ptrauth_objc_super_pointer',
      '__real',
      '__strong',
      '__unsafe_unretained',
      '__unused',
      '__weak',
    )),

    // --- Storage class specifier extensions ---

    storage_class_specifier: (_, original) => choice(
      original,
      '__inline__',
      'CG_EXTERN',
      'CG_INLINE',
      'FOUNDATION_EXPORT',
      'FOUNDATION_EXTERN',
      'FOUNDATION_STATIC_INLINE',
      'IBOutlet',
      'IBInspectable',
      'IB_DESIGNABLE',
      'NS_INLINE',
      'NS_VALID_UNTIL_END_OF_SCOPE',
      'OBJC_EXPORT',
      'OBJC_ROOT_CLASS',
      'UIKIT_EXTERN',
    ),

    // --- ObjC Expressions ---

    _expression_not_binary: ($, original) => choice(
      original,
      $.message_expression,
      $.selector_expression,
      $.available_expression,
      $.block_literal,
      $.dictionary_literal,
      $.array_literal,
      $.at_expression,
      $.encode_expression,
      $.va_arg_expression,
      $.keyword_identifier,
    ),

    message_expression: $ => prec(PREC.CALL, seq(
      '[',
      field('receiver', choice($.expression, $.generic_specifier)),
      repeat1(seq(
        field('method', $.identifier),
        repeat(seq(
          ':',
          commaSep1($.expression),
        )),
      )),
      ']',
    )),

    selector_expression: $ => prec.left(seq(
      '@selector',
      repeat1('('),
      choice($.identifier, $.method_identifier, prec(-1, /[^)]*/)),
      repeat1(')'),
    )),

    available_expression: $ => seq(
      choice('@available', '__builtin_available'),
      '(',
      commaSep1(choice(
        $.identifier,
        seq($.identifier, $.version),
        '*',
      )),
      ')',
    ),

    block_literal: $ => seq(
      '^',
      optional($.attribute_specifier),
      optional($.type_name),
      optional($.attribute_specifier),
      optional($.parameter_list),
      optional($.attribute_specifier),
      $.compound_statement,
    ),

    at_expression: $ => prec.right(seq(
      '@',
      $.expression,
    )),

    dictionary_literal: $ => seq(
      '@',
      '{',
      optional(seq(
        commaSep1($.dictionary_pair),
        optional(','),
      )),
      '}',
    ),

    dictionary_pair: $ => seq($.expression, ':', $.expression),

    array_literal: $ => seq(
      '@',
      '[',
      optional(seq(
        commaSep1($.expression),
        optional(','),
      )),
      ']',
    ),

    encode_expression: $ => seq('@encode', '(', $.type_name, ')'),

    va_arg_expression: $ => seq('va_arg', '(', $.expression, ',', $.type_descriptor, ')'),

    // Override string_literal to support @"string"
    string_literal: $ => seq(
      choice(seq('@', '"'), 'L"', 'u"', 'U"', 'u8"', '"'),
      repeat(choice(
        alias(token.immediate(prec(1, /[^\\"\n]+/)), $.string_content),
        $.escape_sequence,
      )),
      '"',
    ),

    preproc_include: $ => seq(
      field('directive', choice(preprocessor('include'), preprocessor('import'))),
      field('path', choice(
        $.string_literal,
        $.system_lib_string,
        $.identifier,
        alias($.preproc_call_expression, $.call_expression),
      )),
      token.immediate(/\r?\n/),
    ),
  },
});

// --- Helper functions ---

/**
 * @param {string} suffix
 * @param {any} content
 * @returns {any}
 */
function preprocIf(suffix, content) {
  function elseBlock($) {
    return choice(
      suffix ? alias($['preproc_else' + suffix], $.preproc_else) : $.preproc_else,
      suffix ? alias($['preproc_elif' + suffix], $.preproc_elif) : $.preproc_elif,
    );
  }

  return {
    ['preproc_if' + suffix]: $ => seq(
      preprocessor('if'),
      field('condition', $._preproc_expression),
      '\n',
      repeat(prec(2, content($))),
      field('alternative', optional(elseBlock($))),
      preprocessor('endif'),
    ),

    ['preproc_ifdef' + suffix]: $ => seq(
      choice(preprocessor('ifdef'), preprocessor('ifndef')),
      field('name', $.identifier),
      repeat(prec(2, content($))),
      field('alternative', optional(choice(elseBlock($), $.preproc_elifdef))),
      preprocessor('endif'),
    ),

    ['preproc_else' + suffix]: $ => seq(
      preprocessor('else'),
      repeat(prec(2, content($))),
    ),

    ['preproc_elif' + suffix]: $ => seq(
      preprocessor('elif'),
      field('condition', $._preproc_expression),
      '\n',
      repeat(prec(2, content($))),
      field('alternative', optional(elseBlock($))),
    ),

    ['preproc_elifdef' + suffix]: $ => seq(
      choice(preprocessor('elifdef'), preprocessor('elifndef')),
      field('name', $.identifier),
      repeat(prec(2, content($))),
      field('alternative', optional(elseBlock($))),
    ),
  };
}

/**
 * @param {RegExp | Rule | string} command
 * @returns {AliasRule}
 */
function preprocessor(command) {
  return alias(new RegExp('#[ \t]*' + command), '#' + command);
}

/**
 * @param {Rule} rule
 * @returns {ChoiceRule}
 */
function commaSep(rule) {
  return optional(commaSep1(rule));
}

/**
 * @param {Rule} rule
 * @returns {SeqRule}
 */
function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}
