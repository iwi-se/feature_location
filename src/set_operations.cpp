#include "set_operations.hpp"
#include "node_types.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>
#include <stack>
#include <utility>
#include <vector>

// Assuming Node and Tree classes are defined elsewhere with necessary methods

// Placeholder for calculateDepthProximity function
// double calculateDepthProximity(const std::shared_ptr<Node> &node1, const
// std::shared_ptr<Node> &node2)
// {
//     // Implement depth proximity calculation
//     return 0.0;
// }

// // Placeholder for calculateEnvironmentSimilarity function
// double calculateEnvironmentSimilarity(const std::shared_ptr<Node> &node1,
// const std::shared_ptr<Node> &node2)
// {
//     // Implement environment similarity calculation
//     return 0.0;
// }

MatchesPerFile extractMatchesPerFile(const MatchList &matches, const int &index)
{
  MatchesPerFile matchesPerFile;
  for (const auto &match : matches)
  {
    matchesPerFile.push_back(match[index]);
  }
  return matchesPerFile;
}

void sortByDecisionRatio(MatchList &matches, const Configuration &config)
{
  std::vector<std::pair<Match, double>> matchesWithRatio;

  for (const Match &match : matches)
  {
    // double depthProximity = calculateDepthProximity(pair.first,
    // pair.second); double environmentSimilarity =
    // calculateEnvironmentSimilarity(pair.first, pair.second);
    int size = match[0]->getConnectedLeafWeight();

    // double decisionRatio = 0.2 * depthProximity + 0.3 *
    // environmentSimilarity + 0.5 * std::min((size / 100.0), 1.0);

    double decisionRatio = size;
    matchesWithRatio.push_back(std::make_pair(match, decisionRatio));
  }

  std::sort(
      matchesWithRatio.begin(),
      matchesWithRatio.end(),
      [](const std::pair<Match, double> &a, const std::pair<Match, double> &b)
      { return a.second > b.second; });

  matches.clear();
  for (const auto &pair : matchesWithRatio)
  {
    if (std::get<1>(pair) > config.options.minimumTraceWeight)
    {
      matches.push_back(std::get<0>(pair));
    }
  }
}

bool nonOverlapping(const Match &match, const MatchList &result)
{
  for (const auto &existingMatch : result)
  {
    std::vector<Node::RelativePosition> allRelativePositions;
    for (size_t i = 0; i < match.size(); i++)
    {
      Node::RelativePosition pos
          = match[i]->getRelativePosition(existingMatch[i]);
      allRelativePositions.push_back(pos);
    }
    if (std::ranges::adjacent_find(allRelativePositions,
                                   std::ranges::not_equal_to())
            != allRelativePositions.end()
        || allRelativePositions[0] == Node::RelativePosition::overlapping)
    {
      return false;
    }
  }
  return true;
}

void removeOverlappingPairs(MatchList &matches)
{
  MatchList result;
  for (const auto &match : matches)
  {
    if (nonOverlapping(match, result))
    {
      result.push_back(match);
    }
  }
  matches = result;
}

MatchList matchNodeInTrees(const std::shared_ptr<Node>       &node,
                           std::vector<std::shared_ptr<Node>> trees,
                           const Configuration               &config)
{
  MatchList                         matches;
  std::stack<std::shared_ptr<Node>> stack {};
  if (trees.size() > 0)
  {
    stack.push(trees[0]);
    trees.erase(trees.begin());
  }
  while (!stack.empty())
  {
    auto currentNode = stack.top();
    stack.pop();
    if (currentNode->getSubtreeHash() == node->getSubtreeHash())
    {
      if (trees.empty())
      {
        matches.push_back({ node, currentNode });
      }
      else
      {
        MatchList matchesInTrees = matchNodeInTrees(currentNode, trees, config);
        for (auto &match : matchesInTrees)
        {
          // insert currentNode at the beginning of each match
          match.insert(match.begin(), currentNode);
        }
        matches.insert(
            matches.end(), matchesInTrees.begin(), matchesInTrees.end());
      }
    }
    else if (currentNode->getConnectedLeafWeight()
             >= node->getConnectedLeafWeight())
    {
      for (const auto &child : currentNode->getChildren())
      {
        stack.push(child);
      }
    }
  }

  return matches;
}

MatchList matchTrees(std::vector<std::shared_ptr<Node>> trees,
                     const Configuration               &config)
{
  std::vector<std::vector<std::shared_ptr<Node>>> matches;
  std::stack<std::shared_ptr<Node>>               stack { { trees[0] } };
  while (!stack.empty())
  {
    auto currentNode = stack.top();
    stack.pop();
    MatchList nodeMatches;

    if (isIncludedNodeType(currentNode, config))
    {
      std::vector<std::shared_ptr<Node>> remainingTrees { trees.begin() + 1,
                                                          trees.end() };
      nodeMatches = matchNodeInTrees(currentNode, remainingTrees, config);
    }

    if (nodeMatches.empty() || nodeMatches[0].empty())
    {
      for (const auto &child : currentNode->getChildren())
      {
        stack.push(child);
      }
    }
    else
    {
      for (const auto &match : nodeMatches)
      {
        matches.push_back(match);
      }
    }
  }
  return matches;
}

MatchList intersection(const std::vector<std::shared_ptr<Node>> &nodes,
                       const Configuration                      &config)
{
  if (nodes.size() == 1)
  {
    MatchList matches;
    matches.push_back(nodes);
    return matches;
  }
  MatchList matches = matchTrees(nodes, config);
  sortByDecisionRatio(matches, config);
  removeOverlappingPairs(matches);

  return matches;
}

DifferenceResult
    difference(const std::vector<std::shared_ptr<Node>> &leftFiles,
               const std::vector<std::shared_ptr<Node>> &rightFiles,
               const Configuration                      &config)
{
  auto leftSideIntersection = intersection(leftFiles, config);

  // For now do a cartesian product of the left and right files
  DifferenceResult differenceResult;
  for (size_t index = 0; index < leftFiles.size(); index++)
  {
    FileDifferenceResult fileDifferenceResult;
    fileDifferenceResult.intersection
        = extractMatchesPerFile(leftSideIntersection, index);

    for (const auto &rightFile : rightFiles)
    {
      auto subtraction = intersection({ leftFiles[index], rightFile }, config);
      auto subtractionLeftSide = extractMatchesPerFile(subtraction, 0);
      fileDifferenceResult.subtraction.insert(
          fileDifferenceResult.subtraction.end(),
          subtractionLeftSide.begin(),
          subtractionLeftSide.end());
    }
    differenceResult.result.push_back(fileDifferenceResult);
  }

  return differenceResult;
}
