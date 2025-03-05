#ifndef SET_OPERATIONS_HPP
#define SET_OPERATIONS_HPP

#include "tree.hpp"
#include "configuration.hpp"
#include <vector>
#include <memory>

using Match = std::vector<std::shared_ptr<Node>>;

using MatchList = std::vector<Match>;

using MatchesPerFile = std::vector<std::shared_ptr<Node>>;

MatchesPerFile extract_matches_per_file(const MatchList &matches, const int &index);

MatchList intersection(
    const std::vector<std::shared_ptr<Node>> &nodes1,
    const Configuration &config);

struct FileDifferenceResult
{
    std::vector<std::shared_ptr<Node>> intersection;
    std::vector<std::shared_ptr<Node>> subtraction;
};

struct DifferenceResult {
    std::vector<FileDifferenceResult> result;
    std::filesystem::path relative_path;
};

DifferenceResult difference(
    const std::vector<std::shared_ptr<Node>> &leftFiles,
    const std::vector<std::shared_ptr<Node>> &rightFiles,
    const Configuration &config);

#endif
