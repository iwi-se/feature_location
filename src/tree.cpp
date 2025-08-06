#include "tree.hpp"
#include <algorithm>
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

const std::string &Node::getTag() const
{
  return tag;
}

const std::string &Node::getTsText() const
{
  return tsText;
}

const int &Node::getConnectedLeafWeight()
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

const std::size_t &Node::getSubtreeHash()
{
  if (subtreeHash == 0)
  {
    std::string temp_hash;
    temp_hash = tag;
    if (isLeaf())
    {
      temp_hash += tsText;
    }
    for (auto &child : children)
    {
      temp_hash.append(std::to_string(child->getSubtreeHash()));
    }
    subtreeHash = std::hash<std::string> {}(temp_hash);
    if (subtreeHash == 0) // for the very rare case that the hash is 0
    {
      subtreeHash = 1;
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

Node *Node::getRoot()
{
  if (parent == nullptr)
  {
    return this;
  }
  return parent->getRoot();
}

bool Node::isAncestorOf(Node *node)
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

const std::vector<std::unique_ptr<Node>> &Node::getChildren()
{
  return children;
}

Node::RelativePosition Node::getRelativePosition(Node *other)
{
  if (other == this)
  {
    return RelativePosition::overlapping;
  }
  if (isAncestorOf(other) || other->isAncestorOf(this))
  {
    return RelativePosition::overlapping;
  }
  else
  {
    int compareValue1, compareValue2;
    if (sourcePosition.getStartPosition().first
        == other->sourcePosition.getStartPosition().first)
    {
      compareValue1 = sourcePosition.getStartPosition().second;
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

const SourcePosition &Node::getSourcePosition() const
{
  return sourcePosition;
}

void Node::setNodeTypes(const std::vector<std::string> &types)
{
  allTypes = types;
}

const std::vector<std::string> &Node::getNodeTypes() const
{
  return allTypes;
}

std::vector<Node *> &Node::getLeafs()
{
  if (connectedLeaves.empty())
  {
    if (this->isLeaf())
    {
      connectedLeaves.push_back(this);
    }
    else
    {
      for (auto &child : this->children)
      {
        auto childResult { child->getLeafs() };
        connectedLeaves.insert(
            connectedLeaves.end(), childResult.begin(), childResult.end());
      }
    }
  }
  return connectedLeaves;
}

std::vector<Node *> Node::getAncestors()
{
  std::vector<Node *> ancestors;
  Node               *current = this;
  while (current->parent != nullptr)
  {
    ancestors.push_back(current->parent);
    current = current->parent;
  }
  return ancestors;
}

bool Node::getIsInIntersection()
{
  if (isInIntersection)
  {
    return true;
  }
  for (auto ancestor : this->getAncestors())
  {
    if (ancestor->getIsInIntersection())
    {
      return true;
    }
  }
  return false;
}

void Node::setIsInIntersection()
{
  isInIntersection = true;
  for (auto &child : children)
  {
    child->setIsInIntersection();
  }
}

std::vector<Node *> Node::subtreesNotInIntersection()
{
  std::vector<Node *> result;
  std::vector<Node *> descendants { this->getPointerToEveryNode() };
  if (std::none_of(descendants.begin(),
                   descendants.end(),
                   [](Node *&node) { return node->getIsInIntersection(); }))
  {
    result.push_back(this);
    return result;
  }
  for (auto &child : children)
  {
    auto childResult { child->subtreesNotInIntersection() };
    result.insert(result.end(), childResult.begin(), childResult.end());
  }
  return result;
}

void Node::setFeatureAFfiliations(const std::set<size_t> &featureAffiliations)
{
  this->featureAffiliations = featureAffiliations;
}

std::set<size_t> Node::getFeatureAffiliations()
{
  return this->featureAffiliations;
}
