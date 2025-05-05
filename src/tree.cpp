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
      for (auto &child : children)
      {
        connectedLeafWeight += child->getConnectedLeafWeight();
      }
    }
  }
  return connectedLeafWeight;
}

void Node::addChild(Node *child)
{
  children.push_back(std::unique_ptr<Node>(child));
  child->setParent(this);
}

void Node::setParent(Node *parent)
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
  for (auto &child : children)
  {
    child->render(whitespace + 2);
  }
}

bool Node::isLeaf() const
{
  return children.empty();
}

std::vector<Node *> Node::getPointerToEveryNode()
{
  std::vector<Node *> nodes;
  std::stack<Node *>  stack;
  stack.push(this);

  while (!stack.empty())
  {
    auto current = stack.top();
    stack.pop();
    nodes.push_back(current);

    for (auto it = current->children.rbegin(); it != current->children.rend();
         ++it)
    {
      stack.push(it->get());
    }
  }

  return nodes;
}

const std::string& Node::getSubtreeHash()
{
  if (subtreeHash.empty())
  {
    subtreeHash = tag;
    if (isLeaf())
    {
      subtreeHash += tsText;
    }
    for (auto &child : children)
    {
      subtreeHash += child->getSubtreeHash();
    }
  }
  return subtreeHash;
}

Node *Node::getChildByTag(const std::string &tag)
{
  for (const auto &child : children)
  {
    if (child->getTag() == tag)
    {
      return child.get();
    }
  }
  return nullptr;
}

Node *Node::getParent()
{
  return parent;
}

bool Node::isDescendant(Node *node)
{
  auto current = node;
  while (current->parent != nullptr)
  {
    if (current->parent == this)
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

std::vector<Node *> Node::getChildren()
{
  std::vector<Node *> childs;
  for (const auto &child : children)
  {
    childs.push_back(child.get());
  }
  return childs;
}

Node::RelativePosition Node::getRelativePosition(Node *other)
{
  if (other == this)
  {
    return RelativePosition::overlapping;
  }
  if (isDescendant(other) || other->isDescendant(this))
  {
    return RelativePosition::overlapping;
  }
  else
  {
    int compareValue1, compareValue2;
    if (sourcePosition.getStartPosition().first
        == other->sourcePosition.getStartPosition().first)
    {
      compareValue1
          = sourcePosition.getStartPosition().second;
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

std::vector<std::string> Node::getNodeTypes() const
{
  return allTypes;
}
