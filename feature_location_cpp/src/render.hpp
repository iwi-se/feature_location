#ifndef RENDER_HPP
#define RENDER_HPP

#include "tree.hpp"
#include "configuration.hpp"
#include "set_operations.hpp"

std::string render_difference(DifferenceResult difference, Configuration config);

std::string render_file(std::filesystem::path file, Configuration config, std::vector<std::shared_ptr<Node>> green_nodes, std::vector<std::shared_ptr<Node>> red_nodes = {});

#endif
