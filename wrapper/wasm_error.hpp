#ifndef TREE_SITTER_WASM_ERROR_HPP
#define TREE_SITTER_WASM_ERROR_HPP

#include <string>
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @struct WasmError
 * @brief A simple struct to capture Tree-sitter Wasm error information.
 */
struct WasmError {
  TSWasmErrorKind kind{TSWasmErrorKindNone};
  std::string message;

  WasmError() = default;

  WasmError(const TSWasmError &err) {
    kind = err.kind;
    if (err.message) {
      message = err.message;
    }
  }

  bool ok() const { return (kind == TSWasmErrorKindNone); }

  std::string kind_str() const {
    switch (kind) {
    case TSWasmErrorKindNone:
      return "None";
    case TSWasmErrorKindParse:
      return "Parse";
    case TSWasmErrorKindCompile:
      return "Compile";
    case TSWasmErrorKindInstantiate:
      return "Instantiate";
    case TSWasmErrorKindAllocate:
      return "Allocate";
    default:
      return "Unknown";
    }
  }
};

} // namespace tscpp

#endif // TREE_SITTER_WASM_ERROR_HPP
