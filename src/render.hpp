#ifndef RENDER_HPP
#define RENDER_HPP

#include "configuration.hpp"
#include "set_operations.hpp"
#include "tree.hpp"

std::string renderDifference(DifferenceResult     difference,
                             const Configuration &config);

std::string renderFile(std::filesystem::path              file,
                       const Configuration               &config,
                       std::vector<std::shared_ptr<Node>> greenNodes,
                       std::vector<std::shared_ptr<Node>> redNodes = {});

#endif
