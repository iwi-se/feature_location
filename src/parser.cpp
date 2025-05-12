#include "tree.hpp"
#include "tree_sitter/api.h"
#include "tree_sitter/tree-sitter-cpp.h"
#include "tree_sitter/tree-sitter-java.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

const TSLanguage *tree_sitter_java(void);
const TSLanguage *tree_sitter_cpp(void);

std::string getNodeText(const TSNode &tsNode, const std::string &fileContents)
{
  // Get the byte range for this node
  uint32_t startByte = ts_node_start_byte(tsNode);
  uint32_t endByte   = ts_node_end_byte(tsNode);

  // Extract the text for this node based on its byte range
  return fileContents.substr(startByte, endByte - startByte);
}

Node *convertTsNodeToNode(TSNode                       tsNode,
                          const std::filesystem::path &filepath,
                          const std::string           &fileContents)
{
  // Extract node data
  const char *type    = ts_node_type(tsNode);
  bool        isNamed = ts_node_is_named(tsNode);

  // Create SourcePosition
  SourcePosition sourcePosition = SourcePosition(
      filepath,
      { ts_node_start_point(tsNode).row, ts_node_start_point(tsNode).column },
      { ts_node_end_point(tsNode).row, ts_node_end_point(tsNode).column });

  std::string text = getNodeText(tsNode, fileContents);

  // Create the Node
  auto node = new Node(type, text, type, isNamed, sourcePosition);

  // Recursively add children
  uint32_t childCount = ts_node_child_count(tsNode);
  for (uint32_t i = 0; i < childCount; i++)
  {
    TSNode childTsNode = ts_node_child(tsNode, i);
    auto   childNode = convertTsNodeToNode(childTsNode, filepath, fileContents);
    node->addChild(childNode);
  }

  return node;
}

// Assuming you have a function to initialize the parser with the correct
// language
std::unique_ptr<Node> parseFile(const std::filesystem::path &filename,
                                const std::string           &language)
{
  // Initialize the parser
  TSParser *parser = ts_parser_new();
  if (language == "java")
  {
    ts_parser_set_language(parser, tree_sitter_java());
  }
  else if (language == "cpp")
  {
    ts_parser_set_language(parser, tree_sitter_cpp());
  }
  else
  {
    throw std::runtime_error("Unsupported language: " + language);
    return nullptr;
  }

  // Read the file
  std::ifstream file(filename);
  std::string   code((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());

  if (code.empty()) {
    throw std::runtime_error("File is empty or does not exist: " + filename.string());
  }

  // Parse the code
  TSTree *tree
      = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
  TSNode rootNode = ts_tree_root_node(tree);

  // Convert the root TSNode to our Node structure
  Node *root = convertTsNodeToNode(rootNode, filename, code);

  // Clean up
  ts_tree_delete(tree);
  ts_parser_delete(parser);
  ts_language_delete(tree_sitter_cpp());
  ts_language_delete(tree_sitter_java());

  return std::unique_ptr<Node>(root);
}
