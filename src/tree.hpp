#ifndef TREE_HPP
#define TREE_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class SourcePosition
{
  public:
    SourcePosition(const std::filesystem::path     &file,
                   const std::pair<size_t, size_t> &startPosition,
                   const std::pair<size_t, size_t> &endPosition)
        : file(file)
        , startPosition(startPosition)
        , endPosition(endPosition)
    { }

    std::filesystem::path     getFile() const;
    std::pair<size_t, size_t> getStartPosition() const;
    size_t                    getStartLine() const;
    size_t                    getStartColumn() const;
    std::pair<size_t, size_t> getEndPosition() const;
    size_t                    getEndLine() const;
    size_t                    getEndColumn() const;
    bool                      operator< (const SourcePosition &other) const;
    bool                      operator== (const SourcePosition &other) const;
    std::string               render() const;
  private:
    std::filesystem::path     file;
    std::pair<size_t, size_t> startPosition;
    std::pair<size_t, size_t> endPosition;
};

class Node
{
  public:
    Node(const std::string    &tag,
         const std::string    &tsText,
         const std::string    &tsType,
         const bool           &tsIsNamed,
         const SourcePosition &sourcePosition);

    std::string getTag() const;
    std::string getTsText() const;
    void        addChild(Node *child);
    void        render(const int &whitespace) const;
    bool        isLeaf() const;
    std::vector<Node*> getPointerToEveryNode();
    const std::string                 &getSubtreeHash();
    int                                getConnectedLeafWeight();
    bool                 isDescendant(Node *node);
    const SourcePosition getSourcePosition() const;
    std::vector<Node*> getChildren();
    Node*              getChildByTag(const std::string &tag);
    Node*              getParent();

    enum class RelativePosition
    {
      before,
      after,
      overlapping,
    };

    RelativePosition getRelativePosition(Node *other);

    void setNodeTypes(const std::vector<std::string> &types);
    std::vector<std::string> getNodeTypes() const;
  private:
    Node                              *parent { nullptr };
    std::vector<std::unique_ptr<Node>> children {};
    std::string                        tag;
    std::string                        tsText;
    std::string                        tsType;
    bool                               tsIsNamed;
    int                                connectedLeafWeight {};
    std::string                        subtreeHash {};
    SourcePosition                     sourcePosition;
    std::vector<std::string>           allTypes {};

    void setParent(Node *parent);
};

#endif // TREE_HPP
