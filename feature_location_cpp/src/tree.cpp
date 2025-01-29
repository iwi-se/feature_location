#include "tree.hpp"
#include <iostream>
#include <memory>

Node::Node(const std::string &tag, const std::string &ts_text,
         const std::string &ts_type, const bool &ts_is_named,
         const int &ts_subtree_size, const std::string &subtree_hash,
         const std::vector<SourcePosition> &source_positions)
    : tag(tag), ts_text(ts_text), ts_type(ts_type), ts_is_named(ts_is_named),
      ts_subtree_size(ts_subtree_size), subtree_hash(subtree_hash),
      source_positions(source_positions) {}

void Node::add_child(const std::shared_ptr<Node> &child)
{
    children.push_back(child);
    child->set_parent(std::make_shared<Node>(*this));
}

void Node::set_parent(const std::shared_ptr<Node> &parent)
{
    this->parent = parent;
}

void Node::render(const int &whitespace)
{
    for (int i = 0; i < whitespace; i++)
    {
        std::cout << " ";
    }
    std::cout << tag << std::endl;
    for (auto child : children)
    {
        child->render(whitespace + 2);
    }
}
