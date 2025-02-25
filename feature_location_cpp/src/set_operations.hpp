#ifndef SET_OPERATIONS_HPP
#define SET_OPERATIONS_HPP

#include "tree.hpp"
#include "configuration.hpp"

using MatchList = std::vector<Match>;

using Match = std::vector<std::shared_ptr<Node>>;

MatchList intersection(
    const std::vector<std::shared_ptr<Node>> &nodes1,
    const Configuration& config);

struct DifferenceResult
{
    std::vector<std::shared_ptr<Node>> file1_intersection;
    std::vector<std::shared_ptr<Node>> file1_subtraction;
    std::vector<std::shared_ptr<Node>> file2_intersection;
    std::vector<std::shared_ptr<Node>> file2_subtraction;
};

DifferenceResult difference(const std::shared_ptr<Node> &leftFile1, 
    const std::shared_ptr<Node> &leftFile2, 
    const std::shared_ptr<Node> &rightFile1, 
    const std::shared_ptr<Node> &rightFile2,
    const Configuration& config);

#endif
