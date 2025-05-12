#ifndef NODE_TYPES_HPP
#define NODE_TYPES_HPP

#include "configuration.hpp"
#include "tree.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

std::vector<std::string> getSupertypes(const json        &nodeTypes,
                                       const std::string &type);

bool isIncludedNodeType(Node *node, Configuration &config);

#endif
