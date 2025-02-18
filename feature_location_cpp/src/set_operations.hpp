#ifndef SET_OPERATIONS_HPP
#define SET_OPERATIONS_HPP

#include "tree.hpp"
#include "configuration.hpp"

std::pair<std::vector<SourcePosition>, std::vector<SourcePosition>> intersection(
    const std::shared_ptr<Node> &file1,
    const std::shared_ptr<Node> &file2,
    const Configuration& config);

struct DifferenceResult
{
    std::vector<SourcePosition> file1_intersection;
    std::vector<SourcePosition> file1_subtraction;
    std::vector<SourcePosition> file2_intersection;
    std::vector<SourcePosition> file2_subtraction;
};

DifferenceResult difference(const std::shared_ptr<Node> &leftFile1, 
    const std::shared_ptr<Node> &leftFile2, 
    const std::shared_ptr<Node> &rightFile1, 
    const std::shared_ptr<Node> &rightFile2,
    const Configuration& config);

#endif
