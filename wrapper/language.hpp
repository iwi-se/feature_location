#ifndef TREE_SITTER_LANGUAGE_HPP
#define TREE_SITTER_LANGUAGE_HPP

#include <string>
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @class Language
 * @brief A RAII-style C++ wrapper around TSLanguage references.
 *
 * Uses ts_language_copy() and ts_language_delete() if supported.
 */
class Language {
public:
  Language() : language_(nullptr) {}
  explicit Language(const TSLanguage *lang)
      : language_(lang ? ts_language_copy(lang) : nullptr) {}

  ~Language() {
    if (language_) {
      ts_language_delete(language_);
    }
  }

  Language(const Language &other)
      : language_(other.language_ ? ts_language_copy(other.language_)
                                  : nullptr) {}

  Language &operator=(const Language &other) {
    if (this == &other)
      return *this;

    if (language_) {
      ts_language_delete(language_);
      language_ = nullptr;
    }
    if (other.language_) {
      language_ = ts_language_copy(other.language_);
    }
    return *this;
  }

  Language(Language &&other) noexcept : language_(other.language_) {
    other.language_ = nullptr;
  }

  Language &operator=(Language &&other) noexcept {
    if (this != &other) {
      if (language_) {
        ts_language_delete(language_);
      }
      language_ = other.language_;
      other.language_ = nullptr;
    }
    return *this;
  }

  bool valid() const noexcept { return language_ != nullptr; }
  const TSLanguage *get() const noexcept { return language_; }

  uint32_t version() const {
    return language_ ? ts_language_version(language_) : 0;
  }

  uint32_t symbol_count() const {
    return language_ ? ts_language_symbol_count(language_) : 0;
  }

  std::string name() const {
#if defined(ts_language_name) ||                                               \
    (TREE_SITTER_VERSION_MAJOR >= 0 && TREE_SITTER_VERSION_MINOR >= 20)
    if (auto n = ts_language_name(language_)) {
      return std::string(n);
    }
    return {};
#else
    return {};
#endif
  }

  std::string symbol_name(TSSymbol sym) const {
    if (!language_)
      return {};
    if (auto n = ts_language_symbol_name(language_, sym)) {
      return std::string(n);
    }
    return {};
  }

  TSSymbolType symbol_type(TSSymbol sym) const {
    return language_ ? ts_language_symbol_type(language_, sym)
                     : TSSymbolTypeAuxiliary;
  }

private:
  const TSLanguage *language_;
};

} // namespace tscpp

#endif // TREE_SITTER_LANGUAGE_HPP
