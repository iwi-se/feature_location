#include "tree.hpp"
#include <iostream>
#include <memory>
#include <stack>

std::filesystem::path SourcePosition::get_file() const
{
    return file;
}

std::pair<int, int> SourcePosition::get_start_position() const
{
    return start_position;
}

int SourcePosition::get_start_line() const
{
    return start_position.first;
}

int SourcePosition::get_start_column() const
{
    return start_position.second;
}

std::pair<int, int> SourcePosition::get_end_position() const
{
    return end_position;
}

int SourcePosition::get_end_line() const
{
    return end_position.first;
}

int SourcePosition::get_end_column() const
{
    return end_position.second;
}

bool SourcePosition::operator<(const SourcePosition &other) const
{
    return start_position < other.start_position;
}

bool SourcePosition::operator==(const SourcePosition &other) const
{
    return start_position == other.start_position && end_position == other.end_position && file == other.file;
}

std::string SourcePosition::render() const
{
    return file.string() + "/" + std::to_string(start_position.first) + ":" + std::to_string(start_position.second) + "-" + std::to_string(end_position.first) + ":" + std::to_string(end_position.second);
}

Node::Node(const std::string &tag, const std::string &ts_text,
           const std::string &ts_type, const bool &ts_is_named,
           const SourcePosition &source_position)
    : tag(tag), ts_text(ts_text), ts_type(ts_type), ts_is_named(ts_is_named),
      source_position(source_position) {}

std::string Node::get_tag() const
{
    return tag;
}

std::string Node::get_ts_text() const
{
    return ts_text;
}

int Node::get_connected_leaf_weight()
{
    if (connected_leaf_weight == 0)
    {
        if (is_leaf())
        {
            if (tag == "identifier")
            {
                connected_leaf_weight = 3;
            }
            else if (ts_is_named)
            {
                connected_leaf_weight = 2;
            }
            else
            {
                connected_leaf_weight = 1;
            }
        }
        else
        {
            for (auto child : children)
            {
                connected_leaf_weight += child->get_connected_leaf_weight();
            }
        }
    }
    return connected_leaf_weight;
}

void Node::add_child(const std::shared_ptr<Node> &child)
{
    children.push_back(child);
    child->set_parent(shared_from_this());
}

void Node::set_parent(const std::shared_ptr<Node> &parent)
{
    this->parent = parent;
}

void Node::render(const int &whitespace) const
{
    for (int i = 0; i < whitespace; i++)
    {
        std::cout << " ";
    }
    std::cout << tag;
    if (is_leaf())
    {
        std::cout << ": \"" << ts_text << "\"";
    }
    std::cout << std::endl;
    for (auto child : children)
    {
        child->render(whitespace + 2);
    }
}

bool Node::is_leaf() const
{
    return children.empty();
}

std::vector<std::shared_ptr<Node>> Node::get_pointer_to_every_node()
{
    std::vector<std::shared_ptr<Node>> nodes;
    std::stack<std::shared_ptr<Node>> stack;
    stack.push(shared_from_this());

    while (!stack.empty())
    {
        auto current = stack.top();
        stack.pop();
        nodes.push_back(current);

        for (auto it = current->children.rbegin(); it != current->children.rend(); ++it)
        {
            stack.push(*it);
        }
    }

    return nodes;
}

std::string Node::get_subtree_hash()
{
    if (subtree_hash.empty())
    {
        subtree_hash = tag;
        if (is_leaf())
        {
            subtree_hash += ts_text;
        }
        for (auto child : children)
        {
            subtree_hash += child->get_subtree_hash();
        }
    }
    return subtree_hash;
}

bool Node::is_descendant(const std::shared_ptr<Node> &node)
{
    auto current = node;
    while (current->parent != nullptr)
    {
        if ((current->parent).get() == this)
        {
            return true;
        }
        else {
            current = current->parent;
        }
    }
    return false;
}

std::vector<std::shared_ptr<Node>> Node::get_children()
{
    return children;
}

Node::RelativePosition Node::get_relative_position(const std::shared_ptr<Node> &other)
{
    if (is_descendant(other) || other->is_descendant(shared_from_this()))
    {
        return RelativePosition::overlapping;
    }
    else
    {
        int compare_value_1, compare_value_2;
        if (shared_from_this()->source_position.get_start_position().first == other->source_position.get_start_position().first)
        {
            compare_value_1 = shared_from_this()->source_position.get_start_position().second;
            compare_value_2 = other->source_position.get_start_position().second;
        }
        else
        {
            compare_value_1 = this->source_position.get_start_position().first;
            compare_value_2 = other->source_position.get_start_position().first;
        }

        if (compare_value_1 < compare_value_2)
        {
            return RelativePosition::before;
        }
        else
        {
            return RelativePosition::after;
        }
    }
}

const SourcePosition Node::get_source_position() const
{
    return source_position;
}