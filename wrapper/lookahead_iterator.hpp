#ifndef TREE_SITTER_LOOKAHEAD_ITERATOR_HPP
#define TREE_SITTER_LOOKAHEAD_ITERATOR_HPP

#include "language.hpp"
#include <string>
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @class LookaheadIterator
 * @brief A RAII-style C++ wrapper around TSLookaheadIterator.
 *
 * Can be used to generate valid next symbols for a given parse
 * state in a language, for advanced use cases.
 */
class LookaheadIterator {
public:
  LookaheadIterator() : iterator_(nullptr) {}

  LookaheadIterator(const Language &language, TSStateId state)
      : iterator_(nullptr) {
    if (!language.valid()) {
      throw std::runtime_error("LookaheadIterator: invalid Language.");
    }
    iterator_ = ts_lookahead_iterator_new(language.get(), state);
  }

  ~LookaheadIterator() {
    if (iterator_) {
      ts_lookahead_iterator_delete(iterator_);
    }
  }

  LookaheadIterator(const LookaheadIterator &) = delete;
  LookaheadIterator &operator=(const LookaheadIterator &) = delete;

  LookaheadIterator(LookaheadIterator &&other) noexcept
      : iterator_(other.iterator_) {
    other.iterator_ = nullptr;
  }

  LookaheadIterator &operator=(LookaheadIterator &&other) noexcept {
    if (this != &other) {
      if (iterator_) {
        ts_lookahead_iterator_delete(iterator_);
      }
      iterator_ = other.iterator_;
      other.iterator_ = nullptr;
    }
    return *this;
  }

  bool reset_state(TSStateId state) {
    return iterator_ ? ts_lookahead_iterator_reset_state(iterator_, state)
                     : false;
  }

  bool reset(const Language &language, TSStateId state) {
    if (!iterator_) {
      if (!language.valid())
        return false;
      iterator_ = ts_lookahead_iterator_new(language.get(), state);
      return (iterator_ != nullptr);
    }
    return ts_lookahead_iterator_reset(iterator_, language.get(), state);
  }

  bool next() {
    return iterator_ ? ts_lookahead_iterator_next(iterator_) : false;
  }

  TSSymbol current_symbol() const {
    return iterator_ ? ts_lookahead_iterator_current_symbol(iterator_) : 0;
  }

  std::string current_symbol_name() const {
    if (!iterator_)
      return {};
    const char *sym_name = ts_lookahead_iterator_current_symbol_name(iterator_);
    return sym_name ? std::string(sym_name) : std::string();
  }

  bool valid() const noexcept { return iterator_ != nullptr; }
  TSLookaheadIterator *get() const noexcept { return iterator_; }

private:
  TSLookaheadIterator *iterator_;
};

} // namespace tscpp

#endif // TREE_SITTER_LOOKAHEAD_ITERATOR_HPP
