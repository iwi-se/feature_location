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

    std::filesystem::path get_file() const;
    std::pair<int, int> get_start_position() const;
    int get_start_line() const;
    int get_start_column() const;
    std::pair<int, int> get_end_position() const;
    int get_end_line() const;
    int get_end_column() const;
    bool operator<(const SourcePosition &other) const;
    bool operator==(const SourcePosition &other) const;
    std::string render() const;

private:
    std::filesystem::path file;
    std::pair<int, int> start_position;
    std::pair<int, int> end_position;
};

class Node : public std::enable_shared_from_this<Node>
{
public:
    Node(const std::string &tag, const std::string &ts_text,
         const std::string &ts_type, const bool &ts_is_named,
         const SourcePosition &source_position);

    std::string get_tag();
    std::string get_ts_text();
    void add_child(const std::shared_ptr<Node> &child);
    void render(const int &whitespace);
    bool is_leaf();
    std::vector<std::shared_ptr<Node>> get_pointer_to_every_node();
    std::string get_subtree_hash();
    int get_connected_leaf_weight();
    bool is_descendant(const std::shared_ptr<Node> &node);
    SourcePosition get_source_position();

    enum class RelativePosition
    {
        before,
        after,
        overlapping,
    };

    RelativePosition get_relative_position(const std::shared_ptr<Node> &other);
private:
    std::shared_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
    std::string tag;
    std::string ts_text;
    std::string ts_type;
    bool ts_is_named;
    int connected_leaf_weight {};
    std::string subtree_hash {};
    SourcePosition source_position;

    void set_parent(const std::shared_ptr<Node> &parent);
};

#endif // TREE_HPP