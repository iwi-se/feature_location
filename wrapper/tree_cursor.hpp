#ifndef TREE_SITTER_TREE_CURSOR_HPP
#define TREE_SITTER_TREE_CURSOR_HPP

#include "node.hpp"
#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @class TreeCursor
 * @brief A RAII-style C++ wrapper around TSTreeCursor.
 *
 * This class manages the lifetime of a TSTreeCursor, which allows
 * imperative traversal of a syntax tree.
 */
class TreeCursor {
public:
  explicit TreeCursor(const Node &node)
      : cursor_(ts_tree_cursor_new(node.get())) {}

  TreeCursor() {
    TSTreeCursor tmp{};
    cursor_ = tmp;
  }

  TreeCursor(const TreeCursor &other) {
    cursor_ = ts_tree_cursor_copy(&other.cursor_);
  }

  TreeCursor &operator=(const TreeCursor &other) {
    if (this == &other)
      return *this;
    ts_tree_cursor_delete(&cursor_);
    cursor_ = ts_tree_cursor_copy(&other.cursor_);
    return *this;
  }

  TreeCursor(TreeCursor &&other) noexcept : cursor_(other.cursor_) {
    TSTreeCursor tmp{};
    other.cursor_ = tmp;
  }

  TreeCursor &operator=(TreeCursor &&other) noexcept {
    if (this != &other) {
      ts_tree_cursor_delete(&cursor_);
      cursor_ = other.cursor_;
      TSTreeCursor tmp{};
      other.cursor_ = tmp;
    }
    return *this;
  }

  ~TreeCursor() { ts_tree_cursor_delete(&cursor_); }

  void reset(const Node &node) { ts_tree_cursor_reset(&cursor_, node.get()); }

  bool goto_parent() { return ts_tree_cursor_goto_parent(&cursor_); }

  bool goto_first_child() { return ts_tree_cursor_goto_first_child(&cursor_); }

  bool goto_last_child() { return ts_tree_cursor_goto_last_child(&cursor_); }

  bool goto_next_sibling() {
    return ts_tree_cursor_goto_next_sibling(&cursor_);
  }

  bool goto_previous_sibling() {
    return ts_tree_cursor_goto_previous_sibling(&cursor_);
  }

  Node current_node() const {
    return Node(ts_tree_cursor_current_node(&cursor_));
  }

private:
  TSTreeCursor cursor_;
};

} // namespace tscpp

#endif // TREE_SITTER_TREE_CURSOR_HPP
