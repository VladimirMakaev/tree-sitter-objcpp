; inherits: cpp

; ObjC @ keywords
[
  "@interface"
  "@implementation"
  "@protocol"
  "@end"
  "@class"
  "@import"
  "@compatibility_alias"
  "@autoreleasepool"
  "@synchronized"
  "@try"
  "@catch"
  "@finally"
  "@throw"
  "@selector"
  "@encode"
  "@available"
  "@synthesize"
  "@dynamic"
  "@property"
  "@optional"
  "@required"
] @keyword

; NS_ENUM/NS_OPTIONS macro keywords
[
  "NS_ENUM"
  "NS_OPTIONS"
  "NS_CLOSED_ENUM"
  "NS_ERROR_ENUM"
  "CF_ENUM"
  "CF_OPTIONS"
  "CF_CLOSED_ENUM"
] @keyword

; Visibility specifiers
(visibility_specification) @keyword.modifier

; ObjC type qualifiers
[
  "__covariant"
  "__contravariant"
] @keyword.modifier

; ARC / nullability qualifiers (when used as type_qualifier)
[
  "__strong"
  "__weak"
  "__unsafe_unretained"
  "__autoreleasing"
  "__block"
  "__bridge"
  "__bridge_retained"
  "__bridge_transfer"
  "__kindof"
  "_Nonnull"
  "_Nullable"
  "_Nullable_result"
  "_Null_unspecified"
  "nonnull"
  "nullable"
] @keyword.modifier

; Apple framework storage class specifiers
[
  "IBOutlet"
  "IBInspectable"
  "IB_DESIGNABLE"
] @keyword.modifier

; ObjC built-in types
[
  "BOOL"
  "IMP"
  "SEL"
  "Class"
  "id"
] @type.builtin

; Method declarations
(method_declaration
  ["+" "-"] @keyword.function)

(method_definition
  ["+" "-"] @keyword.function)

; Method names in declarations
(method_declaration
  (identifier) @function.method)

(method_definition
  (identifier) @function.method)

; Message expression method names
(message_expression
  method: (identifier) @function.method)

; Message expression receiver
(message_expression
  receiver: (identifier) @variable)

; Selector expression
(selector_expression
  (method_identifier
    (identifier) @function.method))

; Protocol names
(protocol_declaration
  (identifier) @type)

(protocol_forward_declaration
  (identifier) @type)

(protocol_reference_list
  (identifier) @type)

; Class names in declarations
(class_interface
  (identifier) @type)

(class_implementation
  (identifier) @type)

(class_forward_declaration
  (identifier) @type)

; Category names
(class_interface
  category: (identifier) @label)

(class_implementation
  category: (identifier) @label)

; Superclass
(class_interface
  superclass: (identifier) @type)

(class_implementation
  superclass: (identifier) @type)

; Property attributes
(property_attribute
  (identifier) @property)

; Property name (in struct_declaration context)
(property_declaration
  (struct_declaration
    (struct_declarator
      (identifier) @property)))

; Property name (pointer declarator)
(property_declaration
  (struct_declaration
    (struct_declarator
      (pointer_declarator
        declarator: (identifier) @property))))

; @synthesize/@dynamic property names
(property_implementation
  (identifier) @property)

; Module path
(module_import
  path: (identifier) @module)

; Availability specifiers
(availability_attribute_specifier) @attribute

; @ prefix as punctuation
"@" @punctuation.special

; Block literal ^ as operator
(block_literal
  "^" @operator)

; Block pointer declarator ^ as operator
(block_pointer_declarator
  "^" @operator)

; Dictionary/array literal brackets
(dictionary_literal
  ["{" "}"] @punctuation.bracket)

(array_literal
  ["[" "]"] @punctuation.bracket)

; ObjC string literal @ prefix
(string_literal
  "@" @string.special)

; Version numbers
(version_number) @number

; Platform names
(platform) @constant.builtin
