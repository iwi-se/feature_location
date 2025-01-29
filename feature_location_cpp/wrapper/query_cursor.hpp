#ifndef TREE_SITTER_QUERY_CURSOR_HPP
#define TREE_SITTER_QUERY_CURSOR_HPP

#include "node.hpp"
#include "query.hpp"
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @class QueryCursor
 * @brief A RAII-style C++ wrapper around TSQueryCursor.
 *
 * Manages the lifetime of a TSQueryCursor, providing methods
 * to execute a Query and retrieve the resulting matches/captures.
 */
class QueryCursor {
public:
  QueryCursor() : cursor_(nullptr) {}

  explicit QueryCursor(TSQueryCursor *raw_cursor) : cursor_(raw_cursor) {}

  ~QueryCursor() {
    if (cursor_) {
      ts_query_cursor_delete(cursor_);
    }
  }

  QueryCursor(const QueryCursor &) = delete;
  QueryCursor &operator=(const QueryCursor &) = delete;

  QueryCursor(QueryCursor &&other) noexcept : cursor_(other.cursor_) {
    other.cursor_ = nullptr;
  }

  QueryCursor &operator=(QueryCursor &&other) noexcept {
    if (this != &other) {
      if (cursor_) {
        ts_query_cursor_delete(cursor_);
      }
      cursor_ = other.cursor_;
      other.cursor_ = nullptr;
    }
    return *this;
  }

  void exec(const Query &query, const Node &node) {
    ensure_cursor();
    ts_query_cursor_exec(cursor_, query.get(), node.get());
  }

  void set_byte_range(uint32_t start_byte, uint32_t end_byte) {
    ensure_cursor();
    ts_query_cursor_set_byte_range(cursor_, start_byte, end_byte);
  }

  void set_point_range(TSPoint start_point, TSPoint end_point) {
    ensure_cursor();
    ts_query_cursor_set_point_range(cursor_, start_point, end_point);
  }

  bool did_exceed_match_limit() const {
    return cursor_ ? ts_query_cursor_did_exceed_match_limit(cursor_) : false;
  }

  uint32_t match_limit() const {
    return cursor_ ? ts_query_cursor_match_limit(cursor_) : 0;
  }

  void set_match_limit(uint32_t limit) {
    ensure_cursor();
    ts_query_cursor_set_match_limit(cursor_, limit);
  }

  bool next_match(TSQueryMatch &out_match) {
    ensure_cursor();
    return ts_query_cursor_next_match(cursor_, &out_match);
  }

  void remove_match(uint32_t match_id) {
    ensure_cursor();
    ts_query_cursor_remove_match(cursor_, match_id);
  }

  bool next_capture(TSQueryMatch &out_match, uint32_t &out_capture_idx) {
    ensure_cursor();
    return ts_query_cursor_next_capture(cursor_, &out_match, &out_capture_idx);
  }

  TSQueryCursor *get() const noexcept { return cursor_; }

private:
  void ensure_cursor() {
    if (!cursor_) {
      cursor_ = ts_query_cursor_new();
    }
  }

  TSQueryCursor *cursor_;
};

} // namespace tscpp

#endif // TREE_SITTER_QUERY_CURSOR_HPP
