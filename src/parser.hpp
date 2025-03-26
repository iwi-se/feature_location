#ifndef PARSER_HPP
#define PARSER_HPP

#include "tree.hpp"
#include "tree_sitter/api.h"
#include <filesystem>

std::shared_ptr<Node> parseFile(const std::filesystem::path &filePath,
                                const std::string           &language);

#endif // PARSER_HPP
