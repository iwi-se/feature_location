#ifndef TREE_SITTER_QUERY_HPP
#define TREE_SITTER_QUERY_HPP

#include <stdexcept>
#include <string>
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @class Query
 * @brief A RAII-style C++ wrapper around TSQuery.
 *
 * Manages the lifetime of a TSQuery, which encapsulates
 * one or more search patterns for Tree-sitter.
 */
class Query {
public:
  Query() : query_(nullptr) {}

  Query(const TSLanguage *language, const std::string &source)
      : query_(nullptr) {
    if (!language) {
      throw std::runtime_error("Null TSLanguage* passed to Query constructor.");
    }
    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;

    query_ = ts_query_new(language, source.c_str(),
                          static_cast<uint32_t>(source.size()), &error_offset,
                          &error_type);

    if (!query_ || error_type != TSQueryErrorNone) {
      std::string msg = "Failed to create TSQuery. Error offset: " +
                        std::to_string(error_offset);
      switch (error_type) {
      case TSQueryErrorSyntax:
        msg += " (Syntax error)";
        break;
      case TSQueryErrorNodeType:
        msg += " (Node type error)";
        break;
      case TSQueryErrorField:
        msg += " (Field error)";
        break;
      case TSQueryErrorCapture:
        msg += " (Capture error)";
        break;
      case TSQueryErrorStructure:
        msg += " (Structure error)";
        break;
      case TSQueryErrorLanguage:
        msg += " (Language error)";
        break;
      default:
        msg += " (Unknown error)";
        break;
      }
      if (query_) {
        ts_query_delete(query_);
        query_ = nullptr;
      }
      throw std::runtime_error(msg);
    }
  }

  ~Query() {
    if (query_) {
      ts_query_delete(query_);
    }
  }

  Query(const Query &) = delete;
  Query &operator=(const Query &) = delete;

  Query(Query &&other) noexcept : query_(other.query_) {
    other.query_ = nullptr;
  }

  Query &operator=(Query &&other) noexcept {
    if (this != &other) {
      if (query_) {
        ts_query_delete(query_);
      }
      query_ = other.query_;
      other.query_ = nullptr;
    }
    return *this;
  }

  bool valid() const { return query_ != nullptr; }

  uint32_t pattern_count() const {
    return query_ ? ts_query_pattern_count(query_) : 0;
  }

  uint32_t capture_count() const {
    return query_ ? ts_query_capture_count(query_) : 0;
  }

  uint32_t string_count() const {
    return query_ ? ts_query_string_count(query_) : 0;
  }

  void disable_capture(const std::string &name) {
    if (query_) {
      ts_query_disable_capture(query_, name.c_str(),
                               static_cast<uint32_t>(name.size()));
    }
  }

  void disable_pattern(uint32_t pattern_index) {
    if (query_) {
      ts_query_disable_pattern(query_, pattern_index);
    }
  }

  TSQuery *get() noexcept { return query_; }
  const TSQuery *get() const noexcept { return query_; }

private:
  TSQuery *query_;
};

} // namespace tscpp

#endif // TREE_SITTER_QUERY_HPP
