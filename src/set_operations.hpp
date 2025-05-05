#ifndef SET_OPERATIONS_HPP
#define SET_OPERATIONS_HPP

#include "configuration.hpp"
#include "tree.hpp"
#include <memory>
#include <vector>

using Match = std::vector<Node *>;

using MatchList = std::vector<Match>;

using MatchesPerFile = std::vector<Node *>;

MatchesPerFile extractMatchesPerFile(const MatchList &matches,
                                     const int       &index);

MatchList intersection(const std::vector<Node *> &nodes1,
                       const Configuration       &config);

struct FileDifferenceResult
{
    std::vector<Node *> intersection;
    std::vector<Node *> subtraction;
};

struct DifferenceResult
{
    std::vector<FileDifferenceResult> result;
    std::filesystem::path             relativePath;
};

DifferenceResult
    difference(const std::vector<std::unique_ptr<Node>> &leftFiles,
               const std::vector<std::unique_ptr<Node>> &rightFiles,
               const Configuration                      &config);

#endif
