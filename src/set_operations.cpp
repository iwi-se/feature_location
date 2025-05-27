#include "set_operations.hpp"
#include "node_types.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>
#include <span>
#include <stack>
#include <utility>
#include <vector>

double calculateEnvironmentSimilarity(const Match &match)
{
  std::vector<std::vector<Node *>> environments;
  for (const auto &node : match)
  {
    std::vector<Node *> environment;
    if (node->getParent() != nullptr)
    {
      for (const auto &sibling : node->getParent()->getChildren())
      {
        environment.push_back(sibling.get());
      }
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
      = environments.empty() || environments[0].empty()
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

double calculatePositionSimilarity(const Match &match)
{
  std::vector<double> positionMetrics;
  for (const auto &node : match)
  {
    positionMetrics.push_back(node->getSourcePosition().getStartLine() * 1000
                              + node->getSourcePosition().getStartColumn());
  }
  // get min and max
  double cumulativePosition { 0 };
  for (const auto &pos : positionMetrics)
  {
    cumulativePosition += pos;
  }
  cumulativePosition  /= positionMetrics.size();
  auto   root          = match[0]->getRoot();
  double lastPosition  = root->getSourcePosition().getEndLine() * 1000
                        + root->getSourcePosition().getEndColumn();
  // return the difference between the min and max
  return 1 - (cumulativePosition / lastPosition);
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
    double positionSimilarity    = calculatePositionSimilarity(match);

    double decisionRatio
        = 0.18 * environmentSimilarity + 0.8 * size + 0.02 * positionSimilarity;

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
    if (std::get<1>(pair) >= config.options.minimumTraceWeight)
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

class TreeIndex
{
  public:
    TreeIndex(Node *node)
    {
      const auto &allNodes { node->getPointerToEveryNode() };
      for (const auto &child : allNodes)
      {
        addNode(child->getSubtreeHash(), child);
      }
    }

    const std::vector<Node *> getNodes(const std::size_t &hash) const
    {
      if (index.contains(hash))
      {
        return index.at(hash);
      }
      else
      {
        return std::move(std::vector<Node *> {});
      }
    }

    void addNode(const std::size_t &hash, Node *node)
    {
      if (index.contains(hash))
      {
        index[hash].push_back(node);
      }
      else
      {
        index[hash] = std::vector<Node *> { node };
      }
    }
  private:
    std::unordered_map<std::size_t, std::vector<Node *>> index;
};

MatchList
    matchNodeInTrees(Node                                          *node,
                     std::span<const std::pair<TreeIndex, Node *>> &indices,
                     const Configuration                           &config)
{
  std::unique_ptr<MatchList> matches { std::make_unique<MatchList>() };
  matches->reserve(indices.size());
  matches->push_back({ node });
  for (const auto &index : indices)
  {
    if (index.first.getNodes(node->getSubtreeHash()).empty())
    {
      return MatchList {}; // If there is even one tree that does not contain
                           // the node, there are no matches
    }
    else
    {
      std::unique_ptr<MatchList> newMatches { std::make_unique<MatchList>() };
      newMatches->reserve(
          matches->size()
          * index.first.getNodes(node->getSubtreeHash()).size());
      for (const auto &node : index.first.getNodes(node->getSubtreeHash()))
      {
        for (const auto &match : *matches)
        {
          Match newMatch { match };
          newMatch.push_back(node);
          newMatches->push_back(std::move(newMatch));
        }
      }
      matches = std::move(newMatches);
    }
    sortByDecisionRatio(*matches, config);
    removeOverlappingPairs(*matches);
  }

  return *matches;
}

MatchList matchTrees(const std::vector<std::pair<TreeIndex, Node *>> &indices,
                     Configuration                                   &config)
{
  std::vector<std::vector<Node *>> matches;
  std::stack<Node *>               stack { { indices[0].second } };
  while (!stack.empty())
  {
    auto currentNode = stack.top();
    stack.pop();
    MatchList nodeMatches;

    if (isIncludedNodeType(currentNode, config))
    {
      std::span<const std::pair<TreeIndex, Node *>> remainingIndices(indices);
      remainingIndices = remainingIndices.subspan(1);
      nodeMatches = matchNodeInTrees(currentNode, remainingIndices, config);
    }

    if (nodeMatches.empty() || nodeMatches[0].empty())
    {
      for (const auto &child : currentNode->getChildren())
      {
        stack.push(child.get());
      }
    }
    else
    {
      for (const auto &match : nodeMatches)
      {
        matches.push_back(match);
      }
    }

    if (config.options.debug)
    {
      std::cout << "Current potential matches: " << nodeMatches.size()
                << std::endl;
      for (const auto &match : nodeMatches)
      {
        std::cout << "Match: ";
        for (const auto &node : match)
        {
          std::cout << node->getTag() << " ";
        }
        std::cout << std::endl;
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

MatchList intersection(const std::vector<std::pair<TreeIndex, Node *>> &indices,
                       Configuration                                   &config)
{
  if (indices.size() == 1)
  {
    MatchList matches;
    matches.push_back({ indices[0].second });
    return matches;
  }
  MatchList matches = matchTrees(indices, config);
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
               Configuration                            &config)
{
  std::vector<std::pair<TreeIndex, Node *>> leftSideIndices;
  for (const auto &node : leftFiles)
  {
    leftSideIndices.push_back(
        std::make_pair(TreeIndex(node.get()), node.get()));
  }
  auto leftSideIntersection = intersection(leftSideIndices, config);

  // For now do a cartesian product of the left and right files
  DifferenceResult differenceResult;
  for (size_t index = 0; index < leftFiles.size(); index++)
  {
    FileDifferenceResult fileDifferenceResult;
    fileDifferenceResult.intersection
        = extractMatchesPerFile(leftSideIntersection, index);

    for (const auto &rightFile : rightFiles)
    {
      TreeIndex rightSideIndex(rightFile.get());
      auto      subtraction
          = intersection({ leftSideIndices[index],
                           std::make_pair(rightSideIndex, rightFile.get()) },
                         config);
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
