#ifndef TREE_SITTER_NODE_HPP
#define TREE_SITTER_NODE_HPP

#include <tree_sitter/api.h>

namespace tscpp {

/**
 * @brief A lightweight RAII wrapper around a TSNode.
 */
class Node {
public:
  Node() : node_{} {}
  explicit Node(TSNode node) : node_(node) {}

  TSNode get() const { return node_; }

private:
  TSNode node_;
};

} // namespace tscpp

#endif // TREE_SITTER_NODE_HPP
