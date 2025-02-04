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

std::string get_node_text(const TSNode &ts_node, const std::filesystem::path& file) {
    // Get the byte range for this node
    uint32_t start_byte = ts_node_start_byte(ts_node);
    uint32_t end_byte = ts_node_end_byte(ts_node);

    // Read the file into a string
    std::ifstream file_stream(file, std::ios::binary);
    std::string file_contents((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());

    // Extract the text for this node based on its byte range
    return file_contents.substr(start_byte, end_byte - start_byte);
}

std::shared_ptr<Node> convert_ts_node_to_node(TSNode ts_node, const std::filesystem::path& file) {
    // Extract node data
    const char *type = ts_node_type(ts_node);
    bool is_named = ts_node_is_named(ts_node);

    // Create SourcePosition
    SourcePosition source_position = SourcePosition(file, {ts_node_start_point(ts_node).row, ts_node_start_point(ts_node).column},
                        {ts_node_end_point(ts_node).row, ts_node_end_point(ts_node).column});

    std::string text = get_node_text(ts_node, file);

    // Create the Node
    auto node = std::make_shared<Node>(type, text, type, is_named, source_position);

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


