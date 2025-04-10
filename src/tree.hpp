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

class Node: public std::enable_shared_from_this<Node>
{
  public:
    Node(const std::string    &tag,
         const std::string    &tsText,
         const std::string    &tsType,
         const bool           &tsIsNamed,
         const SourcePosition &sourcePosition);

    std::string getTag() const;
    std::string getTsText() const;
    void        addChild(const std::shared_ptr<Node> &child);
    void        render(const int &whitespace) const;
    bool        isLeaf() const;
    std::vector<std::shared_ptr<Node>> getPointerToEveryNode();
    const std::string&                        getSubtreeHash();
    int                                getConnectedLeafWeight();
    bool                 isDescendant(const std::shared_ptr<Node> &node);
    const SourcePosition getSourcePosition() const;
    std::vector<std::shared_ptr<Node>> getChildren();
    std::shared_ptr<Node>              getChildByTag(const std::string &tag);
    std::shared_ptr<Node>              getParent();

    enum class RelativePosition
    {
      before,
      after,
      overlapping,
    };

    RelativePosition getRelativePosition(const std::shared_ptr<Node> &other);

    void setNodeTypes(const std::vector<std::string> &types);
    std::vector<std::string> getNodeTypes();
  private:
    std::shared_ptr<Node>              parent { nullptr };
    std::vector<std::shared_ptr<Node>> children {};
    std::string                        tag;
    std::string                        tsText;
    std::string                        tsType;
    bool                               tsIsNamed;
    int                                connectedLeafWeight {};
    std::string                        subtreeHash {};
    SourcePosition                     sourcePosition;
    std::vector<std::string>           allTypes {};

    void setParent(const std::shared_ptr<Node> &parent);
};

#endif // TREE_HPP
