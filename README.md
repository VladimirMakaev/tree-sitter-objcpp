# tree-sitter-objcpp

Objective-C++ grammar for [tree-sitter](https://github.com/tree-sitter/tree-sitter).

## Overview

This grammar extends [tree-sitter-cpp](https://github.com/tree-sitter/tree-sitter-cpp) (which extends [tree-sitter-c](https://github.com/tree-sitter/tree-sitter-c)) to support Objective-C++ (`.mm` files). It provides full C and C++ support inherited from upstream, plus comprehensive Objective-C additions.

## Supported Features

### Objective-C Declarations
- `@interface` / `@implementation` / `@end` (classes, categories, extensions)
- `@protocol` (declarations, forward declarations, `@optional`/`@required`)
- `@class` forward declarations
- `@property` with attributes (`nonatomic`, `strong`, `readonly`, `getter=`, etc.)
- `@synthesize` / `@dynamic`
- Instance variables with visibility (`@public`, `@private`, `@protected`, `@package`)
- Method declarations and definitions (`+` class, `-` instance, keyword arguments)

### Objective-C Expressions
- Message sends: `[obj method:arg]`
- ObjC literals: `@"string"`, `@42`, `@YES`, `@[array]`, `@{dict}`, `@(expr)`
- `@selector(method:)`, `@encode(type)`, `@available(iOS 13.0, *)`
- Block literals: `^(int x){ return x; }`

### Objective-C Statements
- `@try` / `@catch` / `@finally` (distinct from C++ `try`/`catch`)
- `@throw`, `@synchronized`, `@autoreleasepool`
- Fast enumeration: `for (id obj in collection)`

### Type System
- Block pointer declarators: `void (^handler)(int)`
- ARC qualifiers: `__strong`, `__weak`, `__unsafe_unretained`, `__autoreleasing`
- Nullability: `_Nonnull`, `_Nullable`, `_Null_unspecified`
- ObjC types: `id`, `BOOL`, `SEL`, `Class`, `IMP`
- Protocol-qualified types: `id<NSCoding>`
- `typeof` / `__typeof__`
- Apple framework specifiers: `IBOutlet`, `IBInspectable`, `FOUNDATION_EXPORT`, etc.

### Full C++ Support (inherited)
Templates, lambdas, concepts, coroutines, modules, namespaces, classes, enums, structured bindings, and all other C++20/23 features from tree-sitter-cpp.

## Usage

```bash
npm install tree-sitter-objcpp
```

```javascript
const Parser = require('tree-sitter');
const ObjCpp = require('tree-sitter-objcpp');

const parser = new Parser();
parser.setLanguage(ObjCpp);

const tree = parser.parse('@interface Foo : NSObject\n- (void)method;\n@end');
console.log(tree.rootNode.toString());
```

## Development

```bash
npm install --ignore-scripts
npx tree-sitter generate
npx tree-sitter test
npx tree-sitter parse examples/bridge.mm
```

## Known Limitations

- Nested message sends at statement start (`[[Foo alloc] init]`) conflict with C++ attributes (`[[deprecated]]`). Workaround: use `[([Foo alloc]) init]` or assign the inner message to a variable.

## File Types

- `.mm` - Objective-C++ source files
- `.h` - Header files (shared with C, C++, ObjC)

## License

MIT
