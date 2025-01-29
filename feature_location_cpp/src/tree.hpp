#ifndef TREE_HPP
#define TREE_HPP

#include <vector>
#include <string>
#include <memory>
#include <filesystem>

class SourcePosition
{
public:
    SourcePosition(const std::filesystem::path &file, const std::pair<int, int> &start_position, const std::pair<int, int> &end_position) : file(file), start_position(start_position), end_position(end_position) {}

private:
    std::filesystem::path file;
    std::pair<int, int> start_position;
    std::pair<int, int> end_position;
};

class Node
{
public:
    Node(const std::string &tag, const std::string &ts_text,
         const std::string &ts_type, const bool &ts_is_named,
         const int &ts_subtree_size, const std::string &subtree_hash,
         const std::vector<SourcePosition> &source_positions);
    void add_child(const std::shared_ptr<Node> &child);
    void render(const int &whitespace);

private:
    std::shared_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
    std::string tag;
    std::string ts_text;
    std::string ts_type;
    bool ts_is_named;
    int ts_subtree_size;
    std::string subtree_hash;
    std::vector<SourcePosition> source_positions;

    void set_parent(const std::shared_ptr<Node> &parent);
};

#endif // TREE_HPP