#ifndef TREE_SITTER_TREE_HPP
#define TREE_SITTER_TREE_HPP

#include <cstdlib>
#include <tree_sitter/api.h>
#include <vector>

namespace tscpp {

/**
 * @brief A RAII wrapper around the TSTree* resource.
 */
class Tree {
public:
  Tree() : tree_(nullptr) {}
  explicit Tree(TSTree *raw_tree) : tree_(raw_tree) {}

  Tree(const Tree &other) {
    if (other.tree_) {
      tree_ = ts_tree_copy(other.tree_);
    } else {
      tree_ = nullptr;
    }
  }

  Tree &operator=(const Tree &other) {
    if (this == &other)
      return *this;

    if (tree_) {
      ts_tree_delete(tree_);
      tree_ = nullptr;
    }
    if (other.tree_) {
      tree_ = ts_tree_copy(other.tree_);
    }
    return *this;
  }

  Tree(Tree &&other) noexcept : tree_(other.tree_) { other.tree_ = nullptr; }

  Tree &operator=(Tree &&other) noexcept {
    if (this != &other) {
      if (tree_) {
        ts_tree_delete(tree_);
      }
      tree_ = other.tree_;
      other.tree_ = nullptr;
    }
    return *this;
  }

  ~Tree() {
    if (tree_) {
      ts_tree_delete(tree_);
    }
  }

  const TSTree *get() const noexcept { return tree_; }
  TSTree *get() noexcept { return tree_; }

  Tree copy() const {
    if (!tree_)
      return Tree(nullptr);
    return Tree(ts_tree_copy(tree_));
  }

  TSNode root_node() const {
    if (!tree_) {
      TSNode null_node{};
      return null_node;
    }
    return ts_tree_root_node(tree_);
  }

  TSNode root_node_with_offset(uint32_t offset_bytes,
                               TSPoint offset_extent) const {
    if (!tree_) {
      TSNode null_node{};
      return null_node;
    }
    return ts_tree_root_node_with_offset(tree_, offset_bytes, offset_extent);
  }

  const TSLanguage *language() const {
    return tree_ ? ts_tree_language(tree_) : nullptr;
  }

  std::vector<TSRange> included_ranges() const {
    std::vector<TSRange> result;
    if (!tree_)
      return result;
    uint32_t length = 0;
    TSRange *ranges = ts_tree_included_ranges(tree_, &length);
    if (ranges) {
      result.assign(ranges, ranges + length);
      free(ranges);
    }
    return result;
  }

  void edit(const TSInputEdit *edit) {
    if (tree_) {
      ts_tree_edit(tree_, edit);
    }
  }

  std::vector<TSRange> get_changed_ranges(const Tree &other) const {
    std::vector<TSRange> result;
    if (!tree_ || !other.tree_) {
      return result;
    }
    uint32_t length = 0;
    TSRange *changed = ts_tree_get_changed_ranges(tree_, other.tree_, &length);
    if (changed) {
      result.assign(changed, changed + length);
      free(changed);
    }
    return result;
  }

  void print_dot_graph(int fd) const {
    if (tree_) {
      ts_tree_print_dot_graph(tree_, fd);
    }
  }

private:
  TSTree *tree_;
};

} // namespace tscpp

#endif // TREE_SITTER_TREE_HPP
