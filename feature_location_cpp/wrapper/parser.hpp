#ifndef TREE_SITTER_PARSER_HPP
#define TREE_SITTER_PARSER_HPP

#include <stdexcept>
#include <tree_sitter/api.h>
#include <vector>

namespace tscpp {

/**
 * @brief A RAII wrapper around the TSParser* resource.
 */
class Parser {
public:
  Parser() : parser_(ts_parser_new()) {
    if (!parser_) {
      throw std::runtime_error("Failed to create TSParser.");
    }
  }

  ~Parser() {
    if (parser_) {
      ts_parser_delete(parser_);
    }
  }

  Parser(const Parser &) = delete;
  Parser &operator=(const Parser &) = delete;

  Parser(Parser &&other) noexcept : parser_(other.parser_) {
    other.parser_ = nullptr;
  }

  Parser &operator=(Parser &&other) noexcept {
    if (this != &other) {
      if (parser_) {
        ts_parser_delete(parser_);
      }
      parser_ = other.parser_;
      other.parser_ = nullptr;
    }
    return *this;
  }

  const TSParser *get() const noexcept { return parser_; }
  TSParser *get() noexcept { return parser_; }

  bool set_language(const TSLanguage *language) {
    if (!language) {
      throw std::runtime_error("Null TSLanguage* passed to set_language().");
    }
    return ts_parser_set_language(parser_, language);
  }

  const TSLanguage *language() const { return ts_parser_language(parser_); }

  bool set_included_ranges(const TSRange *ranges, uint32_t count) {
    return ts_parser_set_included_ranges(parser_, ranges, count);
  }

  std::vector<TSRange> included_ranges() const {
    uint32_t count = 0;
    const TSRange *ptr = ts_parser_included_ranges(parser_, &count);
    return std::vector<TSRange>(ptr, ptr + count);
  }

  TSTree *parse(const TSTree *old_tree, TSInput input) {
    return ts_parser_parse(parser_, old_tree, input);
  }

  TSTree *parse_string(const TSTree *old_tree, const char *string,
                       uint32_t length) {
    return ts_parser_parse_string(parser_, old_tree, string, length);
  }

  TSTree *parse_string_encoding(const TSTree *old_tree, const char *string,
                                uint32_t length, TSInputEncoding encoding) {
    return ts_parser_parse_string_encoding(parser_, old_tree, string, length,
                                           encoding);
  }

  void reset() { ts_parser_reset(parser_); }

private:
  TSParser *parser_;
};

} // namespace tscpp

#endif // TREE_SITTER_PARSER_HPP
