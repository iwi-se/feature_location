#include "set_operations.hpp"
#include "node_types.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <span>
#include <stack>
#include <utility>
#include <vector>

double calculateEnvironmentSimilarity(const Match &match)
{
  if (match.size() < 2)
  {
    throw "Match must have at least two nodes";
  }

  if (match[0]->getConnectedLeafWeight()
      > 3) // This function is very expensive and becomes less important for
           // larger subtrees, so we only run it for small subtrees
  {
    return 1.0;
  }

  // Use first match node as reference
  auto &referenceNode { match[0] };

  unsigned int equalSiblingsCount {};
  unsigned int siblingLeaveCount {};
  if (referenceNode->getParent() != nullptr)
  {
    for (const auto &referenceSibling :
         referenceNode->getParent()->getChildren())
    {
      if (referenceSibling.get() == referenceNode)
      {
        break;
      }
      for (auto &referenceSiblingLeave : referenceSibling->getLeaves())
      {
        siblingLeaveCount++;
        bool isInAllEnvironments { true };
        for (size_t i { 1 }; i < match.size(); ++i)
        {
          bool foundInEnv { false };

          for (const auto &sibling : match[i]->getParent()->getChildren())
          {
            if (sibling.get() == match[i])
            {
              break;
            }
            for (auto &siblingLeave : sibling->getLeaves())
            {
              if (siblingLeave->getSubtreeHash()
                  == referenceSiblingLeave->getSubtreeHash())
              {
                foundInEnv = true;
                break;
              }
            }
            if (foundInEnv)
            {
              break;
            }
          }
          if (!foundInEnv)
          {
            isInAllEnvironments = false;
            break;
          }
        }
        if (isInAllEnvironments)
        {
          equalSiblingsCount++;
        }
      }
    }
  }

  // Normalize by the size of the first environment (or any environment since
  // they should be same size)
  double normalizedCount { siblingLeaveCount == 0
                               ? 1
                               : static_cast<double>(equalSiblingsCount)
                                     / static_cast<double>(siblingLeaveCount) };

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
    if (matches->size() > 100)
    {
      sortByDecisionRatio(*matches, config);
      removeOverlappingPairs(*matches);
    }
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

using MatchListIndex = std::vector<std::vector<size_t>>;

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

bool potentialMatches(Node                               *&node,
                      const std::multimap<size_t, size_t> &index,
                      const std::vector<Node *>           &tokenTable,
                      MatchListIndex                      &matchList)
{
  auto           sth { node->getSubtreeHash() };
  MatchListIndex newMatchList {};

  if (!index.contains(sth))
  {
    return false;
  }

  for (auto [search, rangeEnd] { index.equal_range(sth) }; search != rangeEnd;
       ++search)
  {
    auto newMatches { matchList };
    for (auto &newMatch : newMatches)
    {
      newMatch.push_back(search->second);
      newMatchList.push_back(newMatch);
    }
  }
  matchList = newMatchList;
  return true;
}

std::pair<std::multimap<size_t, size_t>, std::vector<Node *>>
    makeIndex(Node *&tree)
{
  auto leaves { tree->getLeaves() };

  std::multimap<size_t, size_t>
      index {}; // key=subtreeHash, value=Index in leaves

  for (size_t i { 0 }; i < leaves.size(); ++i)
  {
    index.insert(std::make_pair(leaves[i]->getSubtreeHash(), i));
  }

  return make_pair(index, leaves);
}

bool nonOverlapping2(const std::vector<size_t> &match,
                     const MatchListIndex      &result)
{
  for (const auto &existingMatch : result)
  {
    std::vector<bool> beforeOrAfter;
    for (size_t i { 0 }; i < match.size(); ++i)
    {
      if (match[i] == existingMatch[i])
      {
        return false;
      }
      if (match[i] < existingMatch[i])
      {
        beforeOrAfter.push_back(true);
      }
      else
      {
        beforeOrAfter.push_back(false);
      }
    }
    if (std::ranges::adjacent_find(beforeOrAfter, std::ranges::not_equal_to())
        != beforeOrAfter.end())
    {
      return false;
    }
  }
  return true;
}

void removeOverlappingPairs2(MatchListIndex &matches)
{
  MatchListIndex result;
  for (const auto &match : matches)
  {
    if (nonOverlapping2(match, result))
    {
      result.push_back(match);
    }
  }
  matches = result;
}

void filterMatches(MatchListIndex     &matchList,
                   std::vector<Node *> tt1,
                   std::vector<Node *> tt2)
{
  size_t                                                       range { 10 };
  std::vector<std::pair<std::vector<unsigned long>, unsigned>> results {};

  for (auto &match : matchList)
  {
    auto firstIndex { *match.begin() };
    auto lastIndex { *(match.end() - 1) };

    auto firstRangeMin { firstIndex >= range ? firstIndex - range : 0 };
    auto firstRangeMax { firstIndex + range <= tt1.size() - 1
                             ? firstIndex + range
                             : tt1.size() - 1 };
    auto lastRangeMin { lastIndex >= range ? lastIndex - range : 0 };
    auto lastRangeMax { lastIndex + range <= tt2.size() - 1 ? lastIndex + range
                                                            : tt2.size() - 1 };
    auto currentFirstIndex { firstIndex };
    auto currentLastIndex { lastIndex };

    unsigned result { 0 };
    // go backwards first
    while (true)
    {
      if (currentFirstIndex <= firstRangeMin
          || currentLastIndex <= lastRangeMin)
      {
        break;
      }
      currentFirstIndex--;
      currentLastIndex--;
      if (tt1[currentFirstIndex]->getSubtreeHash()
          == tt2[currentLastIndex]->getSubtreeHash())
      {
        result++;
      }
    }
    currentFirstIndex = firstIndex;
    currentLastIndex  = lastIndex;
    // now go forward
    while (true)
    {
      if (currentFirstIndex >= firstRangeMax
          || currentLastIndex >= lastRangeMax)
      {
        break;
      }
      currentFirstIndex++;
      currentLastIndex++;
      if (tt1[currentFirstIndex]->getSubtreeHash()
          == tt2[currentLastIndex]->getSubtreeHash())
      {
        result++;
      }
    }
    results.push_back(std::make_pair(match, result));
  }

  std::sort(results.begin(),
            results.end(),
            [](const std::pair<std::vector<unsigned long>, unsigned> &a,
               const std::pair<std::vector<unsigned long>, unsigned> &b)
            {
              if (a.second == b.second)
              {
                return a.first > b.first;
              }
              else
              {
                return a.second > b.second;
              }
            });
  std::cout << results[0].first[0] << " "
            << tt1[results[0].first[0]]->getTsText() << " "
            << results[0].first[1] << " "
            << tt2[results[0].first[1]]->getTsText() << results[0].second
            << std::endl;

  MatchListIndex resultsSorted;
  for (auto &el : results)
  {
    resultsSorted.push_back(el.first);
  }

  removeOverlappingPairs2(resultsSorted);
  matchList = resultsSorted;
}

MatchList intersection2(const std::vector<std::unique_ptr<Node>> &files,
                        Configuration                            &config)
{
  auto           firstTree { files[0].get() };
  MatchListIndex allPotentialMatches {};
  std::vector<std::pair<std::multimap<size_t, size_t>, std::vector<Node *>>>
      indexTokenTableList {};
  for (auto &file : files)
  {
    auto tree { file.get() };
    indexTokenTableList.push_back(makeIndex(tree));
  }
  auto firstIndexAndTokenTable { indexTokenTableList[0] };

  for (size_t i {}; i < firstIndexAndTokenTable.second.size(); ++i)
  {
    bool           goOn { false };
    MatchListIndex tokenPotentialMatches { { i } };
    for (size_t j { 1 }; j < files.size(); ++j)
    {
      auto currentTree { files[j].get() };

      goOn = potentialMatches(firstIndexAndTokenTable.second[i],
                              indexTokenTableList[j].first,
                              indexTokenTableList[j].second,
                              tokenPotentialMatches);
      filterMatches(tokenPotentialMatches,
                    firstIndexAndTokenTable.second,
                    indexTokenTableList[j].second);
      if (!goOn)
      {
        break;
      }
    }
    if (goOn)
    {
      for (auto &match : tokenPotentialMatches)
      {
        allPotentialMatches.push_back(match);
      }
    }
  }
  filterMatches(allPotentialMatches,
                indexTokenTableList[0].second,
                indexTokenTableList[indexTokenTableList.size() - 1].second);
  for (auto &match : allPotentialMatches)
  {
    for (auto &index : match)
    {
      std::cout << index << " ";
    }
    std::cout << std::endl;
  }

  MatchList ml {};
  for (auto &match : allPotentialMatches)
  {
    Match m {};
    for (size_t i { 0 }; i < match.size(); ++i)
    {
      m.push_back(indexTokenTableList[i].second[match[i]]);
    }
    ml.push_back(m);
  }
  return ml;
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
  auto leftSideIntersection = intersection2(leftFiles, config);

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
