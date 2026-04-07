; inherits: cpp

; Class definitions
(class_interface
  (identifier) @name) @definition.class

(class_implementation
  (identifier) @name) @definition.class

; Protocol definitions
(protocol_declaration
  (identifier) @name) @definition.interface

; Method declarations
(method_declaration
  (identifier) @name) @definition.method

; Method definitions
(method_definition
  (identifier) @name) @definition.method

; Property declarations
(property_declaration
  (struct_declaration
    (struct_declarator
      (identifier) @name))) @definition.field

(property_declaration
  (struct_declaration
    (struct_declarator
      (pointer_declarator
        declarator: (identifier) @name)))) @definition.field

; Message sends (references)
(message_expression
  receiver: (identifier) @name) @reference.call

(message_expression
  method: (identifier) @name) @reference.call

; Module imports
(module_import
  path: (identifier) @name) @reference.module
