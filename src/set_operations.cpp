#include "set_operations.hpp"
#include "node_types.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>
#include <stack>
#include <utility>
#include <vector>

double calculateEnvironmentSimilarity(const Match &match)
{
  std::vector<std::vector<Node *>> environments;
  for (const auto &node : match)
  {
    std::vector<Node *> environment;
    for (const auto &sibling : node->getParent()->getChildren())
    {
      environment.push_back(sibling);
    }
    environments.push_back(environment);
  }

  // Count siblings that have equal tsText across all environments
  int equalSiblingsCount = 0;

  // Use the first environment as reference
  if (!environments.empty())
  {
    for (const auto &referenceSibling : environments[0])
    {
      bool isEqualInAllEnvironments = true;

      // Check if this sibling exists with same tsText in all other environments
      for (size_t i = 1; i < environments.size(); ++i)
      {
        bool foundEqual = false;
        for (const auto &sibling : environments[i])
        {
          if (referenceSibling->getTsText() == sibling->getTsText())
          {
            foundEqual = true;
            break;
          }
        }
        if (!foundEqual)
        {
          isEqualInAllEnvironments = false;
          break;
        }
      }

      if (isEqualInAllEnvironments)
      {
        equalSiblingsCount++;
      }
    }
  }

  // Normalize by the size of the first environment (or any environment since
  // they should be same size)
  double normalizedCount
      = environments.empty()
            ? 0.0
            : static_cast<double>(equalSiblingsCount) / environments[0].size();

  return normalizedCount;
}

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
    // pair.second);
    double environmentSimilarity = calculateEnvironmentSimilarity(match);
    int    size                  = match[0]->getConnectedLeafWeight();

    double decisionRatio = 0.2 * environmentSimilarity + 0.8 * size;

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

MatchList matchNodeInTrees(Node                *node,
                           std::vector<Node *>  trees,
                           const Configuration &config)
{
  MatchList                         matches;
  std::stack<Node *> stack {};
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
          match.insert(match.begin(), node);
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

MatchList matchTrees(std::vector<Node *> trees, const Configuration &config)
{
  std::vector<std::vector<Node *>> matches;
  std::stack<Node *>               stack { { trees[0] } };
  while (!stack.empty())
  {
    auto currentNode = stack.top();
    stack.pop();
    MatchList nodeMatches;

    if (isIncludedNodeType(currentNode, config)
        && currentNode->getConnectedLeafWeight()
               >= config.options.minimumTraceWeight)
    {
      std::vector<Node *> remainingTrees { trees.begin() + 1, trees.end() };
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

void debugOutputMatches(const MatchList &matches)
{
  for (const auto &match : matches)
  {
    std::cout << match[0]->getTag() << " " << match[0]->getTsText() << " ";
    for (const auto &node : match)
    {
      std::cout << node->getSourcePosition().render() << "||";
    }
    std::cout << std::endl;
  }
}

MatchList intersection(const std::vector<Node *> &nodes,
                       const Configuration       &config)
{
  if (nodes.size() == 1)
  {
    MatchList matches;
    matches.push_back(nodes);
    return matches;
  }
  MatchList matches = matchTrees(nodes, config);
  if (config.options.debug)
  {
    std::cout << "Potential Matches: " << matches.size() << std::endl;
    debugOutputMatches(matches);
  }
  sortByDecisionRatio(matches, config);
  removeOverlappingPairs(matches);
  if (config.options.debug)
  {
    std::cout << "Final Matches: " << matches.size() << std::endl;
    debugOutputMatches(matches);
  }

  return matches;
}

DifferenceResult
    difference(const std::vector<std::unique_ptr<Node>> &leftFiles,
               const std::vector<std::unique_ptr<Node>> &rightFiles,
               const Configuration                      &config)
{
  std::vector<Node *> leftSideNodes;
  for (const auto &node : leftFiles)
  {
    leftSideNodes.push_back(node.get());
  }
  auto leftSideIntersection = intersection(leftSideNodes, config);

  // For now do a cartesian product of the left and right files
  DifferenceResult differenceResult;
  for (size_t index = 0; index < leftFiles.size(); index++)
  {
    FileDifferenceResult fileDifferenceResult;
    fileDifferenceResult.intersection
        = extractMatchesPerFile(leftSideIntersection, index);

    for (const auto &rightFile : rightFiles)
    {
      auto subtraction
          = intersection({ leftSideNodes[index], rightFile.get() }, config);
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
