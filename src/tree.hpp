#ifndef TREE_HPP
#define TREE_HPP

#include <filesystem>
#include <memory>
#include <set>
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

    const std::string    &getTag() const;
    const std::string    &getTsText() const;
    void                  addChild(Node *child);
    void                  render(const int &whitespace) const;
    bool                  isLeaf() const;
    std::vector<Node *>   getPointerToEveryNode();
    const std::size_t    &getSubtreeHash();
    const int            &getConnectedLeafWeight();
    bool                  isAncestorOf(Node *node);
    std::vector<Node *>   getAncestors();
    const SourcePosition &getSourcePosition() const;
    const std::vector<std::unique_ptr<Node>> &getChildren();
    Node                *getChildByTag(const std::string &tag);
    Node                *getParent();
    Node                *getRoot();
    std::vector<Node *> &getLeafs();
    bool                 getIsInIntersection();
    void                 setIsInIntersection();
    std::vector<Node *>  subtreesNotInIntersection();

    enum class RelativePosition
    {
      before,
      after,
      overlapping,
    };

    RelativePosition getRelativePosition(Node *other);

    void setNodeTypes(const std::vector<std::string> &types);
    const std::vector<std::string> &getNodeTypes() const;
    size_t                          weight {};
    void setFeatureAFfiliations(const std::set<size_t> &featureAffiliations);
    std::set<size_t> getFeatureAffiliations();
    std::string      getNodeRep();
  private:
    Node                              *parent { nullptr };
    std::vector<std::unique_ptr<Node>> children {};
    std::string                        tag;
    std::string                        tsText;
    std::string                        tsType;
    bool                               tsIsNamed;
    int                                connectedLeafWeight {};
    std::size_t                        subtreeHash {};
    SourcePosition                     sourcePosition;
    std::vector<std::string>           allTypes {};
    std::vector<Node *>                connectedLeaves {};
    bool                               isInIntersection { false };
    std::set<size_t>                   featureAffiliations {};

    void setParent(Node *parent);
};

#endif // TREE_HPP
