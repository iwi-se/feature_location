#ifndef TREE_SITTER_WASM_STORE_HPP
#define TREE_SITTER_WASM_STORE_HPP

#include "wasm_error.hpp"
#include <stdexcept>
#include <string>
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @class WasmStore
 * @brief A RAII wrapper around TSWasmStore* for WebAssembly-based languages.
 */
class WasmStore {
public:
  WasmStore() : store_(nullptr) {}

  explicit WasmStore(TSWasmEngine *engine) : store_(nullptr) {
    if (!engine) {
      throw std::runtime_error("WasmStore: Received null TSWasmEngine*.");
    }
    TSWasmError err{};
    store_ = ts_wasm_store_new(engine, &err);
    if (!store_ || !WasmError(err).ok()) {
      std::string msg = "Failed to create TSWasmStore. Error kind: " +
                        WasmError(err).kind_str() +
                        " Message: " + WasmError(err).message;
      if (store_) {
        ts_wasm_store_delete(store_);
        store_ = nullptr;
      }
      throw std::runtime_error(msg);
    }
  }

  ~WasmStore() {
    if (store_) {
      ts_wasm_store_delete(store_);
    }
  }

  WasmStore(const WasmStore &) = delete;
  WasmStore &operator=(const WasmStore &) = delete;

  WasmStore(WasmStore &&other) noexcept : store_(other.store_) {
    other.store_ = nullptr;
  }

  WasmStore &operator=(WasmStore &&other) noexcept {
    if (this != &other) {
      if (store_) {
        ts_wasm_store_delete(store_);
      }
      store_ = other.store_;
      other.store_ = nullptr;
    }
    return *this;
  }

  bool valid() const noexcept { return store_ != nullptr; }

  const TSLanguage *load_language(const std::string &name, const char *wasm,
                                  uint32_t wasm_len) {
    if (!valid()) {
      throw std::runtime_error(
          "WasmStore::load_language called on invalid store.");
    }
    TSWasmError err{};
    auto lang =
        ts_wasm_store_load_language(store_, name.c_str(), wasm, wasm_len, &err);
    if (!lang || !WasmError(err).ok()) {
      std::string msg = "Failed to load WASM language. Error kind: " +
                        WasmError(err).kind_str() +
                        " Message: " + WasmError(err).message;
      throw std::runtime_error(msg);
    }
    return lang;
  }

  size_t language_count() const {
    return valid() ? ts_wasm_store_language_count(store_) : 0;
  }

  TSWasmStore *get() const noexcept { return store_; }

private:
  TSWasmStore *store_;
};

} // namespace tscpp

#endif // TREE_SITTER_WASM_STORE_HPP
