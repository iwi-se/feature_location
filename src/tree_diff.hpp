#include "tree.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

struct Edit
{
    enum Type
    {
      Insert,
      Delete
    } type;

    Node* node;
};

using EditScript = std::vector<Edit>;

// Recursive function to generate the edit script
void diffNodes(Node* oldNode, Node* newNode, EditScript& edits)
{
  if (!oldNode && newNode)
  {
    edits.push_back({ Edit::Insert, newNode });
    for (const auto& child : newNode->getChildren())
    {
      diffNodes(nullptr, child.get(), edits);
    }
  }
  else if (oldNode && !newNode)
  {
    edits.push_back({ Edit::Delete, const_cast<Node*>(oldNode) });
    for (const auto& child : oldNode->getChildren())
    {
      diffNodes(child.get(), nullptr, edits);
    }
  }
  else if (oldNode && newNode)
  {
    if (oldNode->getTag() != newNode->getTag())
    {
      edits.push_back({ Edit::Delete, const_cast<Node*>(oldNode) });
      edits.push_back({ Edit::Insert, const_cast<Node*>(newNode) });
    }

    auto maxSize = std::max(oldNode->getChildren().size(),
                            newNode->getChildren().size());
    for (size_t i = 0; i < maxSize; ++i)
    {
      Node* oldChild = (i < oldNode->getChildren().size())
                           ? oldNode->getChildren()[i].get()
                           : nullptr;
      Node* newChild = (i < newNode->getChildren().size())
                           ? newNode->getChildren()[i].get()
                           : nullptr;
      diffNodes(oldChild, newChild, edits);
    }
  }
}

// Public interface
EditScript diffTrees(Node* oldRoot, Node* newRoot)
{
  EditScript edits;
  diffNodes(oldRoot, newRoot, edits);
  return edits;
}
