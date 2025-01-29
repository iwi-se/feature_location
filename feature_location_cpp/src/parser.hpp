#ifndef PARSER_HPP
#define PARSER_HPP

#include "tree.hpp"
#include "tree_sitter/api.h"
#include <filesystem>

std::shared_ptr<Node> parse_file(const std::filesystem::path &file_path);

#endif // PARSER_HPP