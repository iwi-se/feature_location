#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include "tree_sitter/api.h"
#include "tree_sitter/tree-sitter-cpp.h"
#include "tree.hpp"

const TSLanguage *tree_sitter_cpp();

std::shared_ptr<Node> convert_ts_node_to_node(TSNode ts_node, const std::filesystem::path& file) {
    // Extract node data
    const char *type = ts_node_type(ts_node);
    const char *text = ts_node_type(ts_node);
    bool is_named = ts_node_is_named(ts_node);
    uint32_t subtree_size = ts_node_child_count(ts_node);
    std::string subtree_hash = std::to_string(ts_node_start_byte(ts_node)) + "-" + std::to_string(ts_node_end_byte(ts_node));

    // Create SourcePosition
    std::vector<SourcePosition> source_positions = {
        SourcePosition(file, {ts_node_start_point(ts_node).row, ts_node_start_point(ts_node).column},
                        {ts_node_end_point(ts_node).row, ts_node_end_point(ts_node).column})
    };

    // Create the Node
    auto node = std::make_shared<Node>(type, text, type, is_named, subtree_size, subtree_hash, source_positions);

    // Recursively add children
    uint32_t child_count = ts_node_child_count(ts_node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child_ts_node = ts_node_child(ts_node, i);
        auto child_node = convert_ts_node_to_node(child_ts_node, file);
        node->add_child(child_node);
    }

    return node;
}

// Assuming you have a function to initialize the parser with the correct language
std::shared_ptr<Node> parse_file(const std::filesystem::path& filename) {
    // Initialize the parser
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_cpp());

    // Read the file
    std::ifstream file(filename);
    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Parse the code
    TSTree *tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
    TSNode root_node = ts_tree_root_node(tree);

    // Convert the root TSNode to our Node structure
    std::shared_ptr<Node> root = convert_ts_node_to_node(root_node, filename);

    // Clean up
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return root;
}


