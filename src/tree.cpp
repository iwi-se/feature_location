#include "tree.hpp"
#include <iostream>
#include <memory>
#include <stack>

std::filesystem::path SourcePosition::getFile() const
{
  return file;
}

std::pair<size_t, size_t> SourcePosition::getStartPosition() const
{
  return startPosition;
}

size_t SourcePosition::getStartLine() const
{
  return startPosition.first;
}

size_t SourcePosition::getStartColumn() const
{
  return startPosition.second;
}

std::pair<size_t, size_t> SourcePosition::getEndPosition() const
{
  return endPosition;
}

size_t SourcePosition::getEndLine() const
{
  return endPosition.first;
}

size_t SourcePosition::getEndColumn() const
{
  return endPosition.second;
}

bool SourcePosition::operator< (const SourcePosition &other) const
{
  return startPosition < other.startPosition;
}

bool SourcePosition::operator== (const SourcePosition &other) const
{
  return startPosition == other.startPosition
         && endPosition == other.endPosition && file == other.file;
}

std::string SourcePosition::render() const
{
  return file.string() + "/" + std::to_string(startPosition.first) + ":"
         + std::to_string(startPosition.second) + "-"
         + std::to_string(endPosition.first) + ":"
         + std::to_string(endPosition.second);
}

Node::Node(const std::string    &tag,
           const std::string    &tsText,
           const std::string    &tsType,
           const bool           &tsIsNamed,
           const SourcePosition &sourcePosition)
    : tag(tag)
    , tsText(tsText)
    , tsType(tsType)
    , tsIsNamed(tsIsNamed)
    , sourcePosition(sourcePosition)
{ }

std::string Node::getTag() const
{
  return tag;
}

std::string Node::getTsText() const
{
  return tsText;
}

int Node::getConnectedLeafWeight()
{
  if (connectedLeafWeight == 0)
  {
    if (isLeaf())
    {
      if (tag == "identifier")
      {
        connectedLeafWeight = 3;
      }
      else if (tsIsNamed)
      {
        connectedLeafWeight = 2;
      }
      else
      {
        connectedLeafWeight = 1;
      }
    }
    else
    {
      for (auto child : children)
      {
        connectedLeafWeight += child->getConnectedLeafWeight();
      }
    }
  }
  return connectedLeafWeight;
}

void Node::addChild(const std::shared_ptr<Node> &child)
{
  children.push_back(child);
  child->setParent(shared_from_this());
}

void Node::setParent(const std::shared_ptr<Node> &parent)
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
  if (isLeaf())
  {
    std::cout << ": \"" << tsText << "\"";
  }
  std::cout << std::endl;
  for (auto child : children)
  {
    child->render(whitespace + 2);
  }
}

bool Node::isLeaf() const
{
  return children.empty();
}

std::vector<std::shared_ptr<Node>> Node::getPointerToEveryNode()
{
  std::vector<std::shared_ptr<Node>> nodes;
  std::stack<std::shared_ptr<Node>>  stack;
  stack.push(shared_from_this());

  while (!stack.empty())
  {
    auto current = stack.top();
    stack.pop();
    nodes.push_back(current);

    for (auto it = current->children.rbegin(); it != current->children.rend();
         ++it)
    {
      stack.push(*it);
    }
  }

  return nodes;
}

std::string Node::getSubtreeHash()
{
  if (subtreeHash.empty())
  {
    subtreeHash = tag;
    if (isLeaf())
    {
      subtreeHash += tsText;
    }
    for (auto child : children)
    {
      subtreeHash += child->getSubtreeHash();
    }
  }
  return subtreeHash;
}

std::shared_ptr<Node> Node::getChildByTag(const std::string &tag)
{
  for (auto child : children)
  {
    if (child->getTag() == tag)
    {
      return child;
    }
  }
  return nullptr;
}

std::shared_ptr<Node> Node::getParent()
{
  return parent;
}

bool Node::isDescendant(const std::shared_ptr<Node> &node)
{
  auto current = node;
  while (current->parent != nullptr)
  {
    if ((current->parent).get() == this)
    {
      return true;
    }
    else
    {
      current = current->parent;
    }
  }
  return false;
}

std::vector<std::shared_ptr<Node>> Node::getChildren()
{
  return children;
}

Node::RelativePosition
    Node::getRelativePosition(const std::shared_ptr<Node> &other)
{
  if (other.get() == this)
  {
    return RelativePosition::overlapping;
  }
  if (isDescendant(other) || other->isDescendant(shared_from_this()))
  {
    return RelativePosition::overlapping;
  }
  else
  {
    int compareValue1, compareValue2;
    if (shared_from_this()->sourcePosition.getStartPosition().first
        == other->sourcePosition.getStartPosition().first)
    {
      compareValue1
          = shared_from_this()->sourcePosition.getStartPosition().second;
      compareValue2 = other->sourcePosition.getStartPosition().second;
    }
    else
    {
      compareValue1 = this->sourcePosition.getStartPosition().first;
      compareValue2 = other->sourcePosition.getStartPosition().first;
    }

    if (compareValue1 < compareValue2)
    {
      return RelativePosition::before;
    }
    else
    {
      return RelativePosition::after;
    }
  }
}

const SourcePosition Node::getSourcePosition() const
{
  return sourcePosition;
}

void Node::setNodeTypes(const std::vector<std::string> &types)
{
  allTypes = types;
}

std::vector<std::string> Node::getNodeTypes()
{
  return allTypes;
}
