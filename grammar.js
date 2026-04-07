/**
 * @file Objective-C++ grammar for tree-sitter
 * @license MIT
 */

const CPP = require('tree-sitter-cpp/grammar');

module.exports = grammar(CPP, {
  name: 'objcpp',

  rules: {
    _top_level_item: ($, original) => original,
  },
});
