#include "set_operations.hpp"
#include "node_types.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <stack>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using IndexWithTokenTable = std::pair<size_t, std::vector<Node *>>;

double calculateEnvironmentSimilarity(const Match &match)
{
  if (match.size() < 2)
  {
    throw "Match must have at least two nodes";
  }

  if (match[0]->getConnectedLeafWeight()
      > 10) // This function is very expensive and becomes less important for
            // larger subtrees, so we only run it for small subtrees
  {
    return 1.0;
  }

  size_t range { 20 };
  size_t result {};
  size_t maxEnvCount {};

  std::vector<IndexWithTokenTable> backwardsIterators;
  std::vector<IndexWithTokenTable> forwardsIterators;
  for (auto &node : match)
  {
    auto  tokenTable { node->getRoot()->getLeaves() };
    auto &nodeTokens { node->getLeaves() };
    auto  firstToken { nodeTokens[0] };
    auto  lastToken { nodeTokens[nodeTokens.size() - 1] };
    auto  firstIterator { std::find(
        tokenTable.begin(), tokenTable.end(), firstToken) };
    auto  lastIterator { std::find(
        tokenTable.begin(), tokenTable.end(), lastToken) };
    backwardsIterators.push_back(std::make_pair(
        std::distance(tokenTable.begin(), firstIterator), tokenTable));
    forwardsIterators.push_back(std::make_pair(
        std::distance(tokenTable.begin(), lastIterator), tokenTable));
  }
  // Backwards
  for (size_t i { 0 }; i < range; ++i)
  {
    bool                stop { false };
    std::vector<size_t> hashes {};
    for (auto &backwardsIterator : backwardsIterators)
    {
      if (backwardsIterator.first == 0)
      {
        stop = true;
        break;
      }
      backwardsIterator.first--;
      hashes.push_back(
          backwardsIterator.second[backwardsIterator.first]->getSubtreeHash());
    }
    if (stop)
    {
      break;
    }
    maxEnvCount++;
    bool notEqual { false };
    for (auto &hash : hashes)
    {
      if (hash != hashes[0])
      {
        notEqual = true;
      }
    }
    if (!notEqual)
    {
      result++;
    }
  }
  for (size_t i { 0 }; i < range; ++i)
  {
    bool                stop { false };
    std::vector<size_t> hashes {};
    for (auto &forwardsIterator : forwardsIterators)
    {
      forwardsIterator.first++;
      if (forwardsIterator.first >= forwardsIterator.second.size())
      {
        stop = true;
        break;
      }
      hashes.push_back(
          forwardsIterator.second[forwardsIterator.first]->getSubtreeHash());
    }
    if (stop)
    {
      break;
    }
    maxEnvCount++;
    bool notEqual { false };
    for (auto &hash : hashes)
    {
      if (hash != hashes[0])
      {
        notEqual = true;
      }
    }
    if (!notEqual)
    {
      result++;
    }
  }
  if (match[0]->getTag() == "return_statement")
  {
    std::cout << match[0]->getTsText() << " "
              << match[0]->getSourcePosition().getStartLine() << " "
              << match[1]->getSourcePosition().getStartLine() << " " << result
              << std::endl;
  }
  return static_cast<double>(result) / static_cast<double>(maxEnvCount);
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

MatchList removeOverlappingPairs(MatchList &matches)
{
  MatchList result;
  MatchList removedMatches;
  for (const auto &match : matches)
  {
    if (nonOverlapping(match, result))
    {
      result.push_back(match);
    }
    else
    {
      removedMatches.push_back(match);
    }
  }
  matches = result;
  return removedMatches;
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
    if (matches->size() > 1)
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
    std::cout << currentNode->getTsText()
              << currentNode->getSourcePosition().getStartLine() << std::endl;
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
      sortByDecisionRatio(matches, config);
      auto removed { removeOverlappingPairs(matches) };

      for (auto removedMatch : removed)
      {
        for (const auto &child : removedMatch[0]->getChildren())
        {
          stack.push(child.get());
        }
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
  size_t                                                       range { 20 };
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

    std::set<size_t> allHashesFirst {};
    std::set<size_t> allHashesLast {};

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
      allHashesFirst.insert(tt1[currentFirstIndex]->getSubtreeHash());
      allHashesLast.insert(tt2[currentLastIndex]->getSubtreeHash());
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
      allHashesFirst.insert(tt1[currentFirstIndex]->getSubtreeHash());
      allHashesLast.insert(tt2[currentLastIndex]->getSubtreeHash());
    }

    std::set<size_t> output;
    std::set_intersection(allHashesFirst.begin(),
                          allHashesFirst.end(),
                          allHashesLast.begin(),
                          allHashesLast.end(),
                          std::inserter(output, output.begin()));
    result += output.size();

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

struct Index
{
    std::string identifier;
    size_t      index;

    bool operator== (const Index &other) const
    {
      return identifier == other.identifier && index == other.index;
    }

    bool operator< (const Index &other) const
    {
      return std::tie(identifier, index)
             < std::tie(other.identifier, other.index);
    }
};

struct LCSTokenIndex
{
    std::string        token;
    std::vector<Index> indexList;

    bool operator== (const LCSTokenIndex &other) const
    {
      return token == other.token && indexList == other.indexList;
    }

    bool operator< (const LCSTokenIndex &other) const
    {
      if (token != other.token)
      {
        return token < other.token;
      }
      return indexList < other.indexList;
    }
};

using LCSIndex = std::vector<LCSTokenIndex>;

using LCSToken = std::string;

using LCS = std::vector<LCSToken>;

bool operator< (const LCS &a, const LCS &b)
{
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

std::vector<LCS> allLCS(const std::vector<LCSToken> &a,
                        const std::vector<LCSToken> &b)
{
  size_t n = a.size(), m = b.size();
  // DP table
  std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
  for (size_t i = 1; i <= n; ++i)
  {
    for (size_t j = 1; j <= m; ++j)
    {
      if (a[i - 1] == b[j - 1])
      {
        dp[i][j] = dp[i - 1][j - 1] + 1;
      }
      else
      {
        dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
      }
    }
  }

  std::vector<LCS>                           resultSet;
  std::function<void(size_t, size_t, LCS &)> backtrack
      = [&](size_t i, size_t j, LCS &current)
  {
    if (i == 0 || j == 0)
    {
      LCS lcs = current;
      std::reverse(lcs.begin(), lcs.end());
      resultSet.push_back(lcs);
      return;
    }
    if (a[i - 1] == b[j - 1])
    {
      LCSToken merged { a[i - 1] };
      current.push_back(merged);
      backtrack(i - 1, j - 1, current);
      current.pop_back();
    }
    else
    {
      if (dp[i - 1][j] == dp[i][j])
      {
        backtrack(i - 1, j, current);
      }
      if (dp[i][j - 1] == dp[i][j])
      {
        backtrack(i, j - 1, current);
      }
    }
  };
  LCS current;
  backtrack(n, m, current);
  return resultSet;
}

// Example: usage and printing
void printLCSs(const std::vector<LCSIndex> &lcss)
{
  for (const auto &lcs : lcss)
  {
    for (const auto &token : lcs)
    {
      std::cout << "\"" << token.token << "\" [";
      for (const auto &idx : token.indexList)
      {
        std::cout << "(" << idx.identifier << "," << idx.index << ")";
      }
      std::cout << "]\n";
    }
    std::cout << "\n\n";
  }
}

LCSToken nodeToLCSToken(Node *node, size_t index)
{
  LCSToken token { node->getTsText() };
  return token;
}

template<typename A>
std::vector<LCSToken> nodeTableToLCSTable(std::vector<A> &nodeTable)
{
  LCS lcs;
  for (size_t i = 0; i < nodeTable.size(); ++i)
  {
    lcs.push_back(nodeToLCSToken(nodeTable[i], i));
  }
  return lcs;
}

std::vector<std::vector<LCSToken>>
    runLCSRecursively(std::vector<std::vector<LCSToken>> lcsTables)
{
  std::vector<std::vector<LCSToken>> currentResult { lcsTables };
  while (currentResult.size() > 1)
  {
    std::vector<std::vector<LCSToken>> newResult {};
    for (size_t i = 0; i < lcsTables.size() - 1; ++i)
    {
      auto result { allLCS(lcsTables[i], lcsTables[i + 1]) };
      std::cout << result.size() << " LCS found" << std::endl;
      newResult.push_back(result[0]);
    }
    currentResult = std::move(newResult);
  }
  return currentResult;
}

std::tuple<bool, LCS, LCS> allLeavesInLCS(const LCS &lcs, Node *subtree)
{
  auto leaves { subtree->getLeaves() };

  std::vector<size_t> lcsIndexes;
  for (size_t i { 0 }; i < lcs.size(); ++i)
  {
    if (lcs[i] == leaves[0]->getTsText())
    {
      lcsIndexes.push_back(i);
    }
  }
  for (auto lcsIndex : lcsIndexes)
  {
    auto   oldIndex { lcsIndex };
    size_t leavesIndex { 0 };
    bool   foundMismatch { false };
    while (lcsIndex < lcs.size() && leavesIndex < leaves.size())
    {
      if (lcs[lcsIndex] != leaves[leavesIndex]->getTsText())
      {
        foundMismatch = true;
        break;
      }
      lcsIndex++;
      leavesIndex++;
    }
    if (!foundMismatch)
    {
      LCS before {};
      if (oldIndex != 0)
      {
        before.insert(before.begin(), lcs.begin(), lcs.begin() + oldIndex - 1);
      }
      LCS after {};
      if (lcsIndex < lcs.size() - 2)
      {
        after.insert(after.begin(), lcs.begin() + lcsIndex, lcs.end());
      }
      return std::make_tuple<bool, LCS, LCS>(
          true, std::move(before), std::move(after));
    }
  }
  return std::make_tuple<bool, LCS, LCS>(false, {}, {});
}

bool notPartOfResult(Node *node, std::vector<Node *> result)
{
  for (auto &resultNode : result)
  {
    if (node == resultNode || node->isAncestorOf(resultNode))
    {
      return false;
    }
  }
  return true;
}

std::tuple<Node *, LCS, LCS> biggestSubtree(LCS lcs, std::vector<Node *> trees)
{
  std::vector<Node *> allSubtrees {};
  for (auto &tree : trees)
  {
    auto subtrees { tree->getPointerToEveryNode() };
    allSubtrees.insert(allSubtrees.end(), subtrees.begin(), subtrees.end());
  }
  std::sort(
      allSubtrees.begin(),
      allSubtrees.end(),
      [](Node *a, Node *b)
      { return a->getConnectedLeafWeight() > b->getConnectedLeafWeight(); });
  for (auto &subtree : allSubtrees)
  {
    auto isAllLeavesInLCS { allLeavesInLCS(
        lcs, subtree) }; // Check if all leaves of
                         // the LCS are in the subtree
    if (std::get<0>(isAllLeavesInLCS))
    {
      return std::make_tuple(subtree,
                             std::get<1>(isAllLeavesInLCS),
                             std::get<2>(isAllLeavesInLCS));
    }
  }
  return std::make_tuple<Node *, LCS, LCS>(nullptr, {}, {});
}

std::vector<Node *> matchLCSToNodesFile(LCS lcs, std::vector<Node *> trees)
{
  auto result { biggestSubtree(lcs, trees) };
  auto biggest { std::get<0>(result) };
  auto lcsBefore { std::get<1>(result) };
  auto lcsAfter { std::get<2>(result) };

  std::cerr << lcs.size() << " " << lcsBefore.size() << " " << lcsAfter.size()
            << std::endl;

  if (lcsBefore.size() != 0 || lcsAfter.size() != 0)
  {
    std::vector<Node *> subtreesBefore {};
    std::vector<Node *> subtreesAfter {};
    auto                current { biggest };
    while (current->getParent() != nullptr)
    {
      bool after { false };
      for (auto &sibling : current->getParent()->getChildren())
      {
        if (sibling.get() == current)
        {
          after = true;
          continue;
        }
        if (after)
        {
          subtreesAfter.push_back(sibling.get());
        }
        else
        {
          subtreesBefore.push_back(sibling.get());
        }
      }
      current = current->getParent();
    }
    auto biggestSubtreesBefore { matchLCSToNodesFile(lcsBefore,
                                                     subtreesBefore) };
    auto biggestSubtreesAfter { matchLCSToNodesFile(lcsAfter, subtreesAfter) };

    auto result { biggestSubtreesBefore };
    result.push_back(biggest);
    result.insert(
        result.end(), biggestSubtreesAfter.begin(), biggestSubtreesAfter.end());
    return result;
  }
  if (biggest != nullptr)
  {
    return { biggest }; // If there are no leaves before or after the LCS,
  }
  else
  {
    return {};
  }
}

MatchList matchLCSToNodes(LCS                                      &lcs,
                          const std::vector<std::unique_ptr<Node>> &files)
{
  std::vector<std::vector<Node *>> leaveNodesPerFile;
  for (auto &file : files)
  {
    std::vector<Node *> nodes { matchLCSToNodesFile(lcs, { file.get() }) };
    std::vector<Node *> leaveNodes {};
    for (auto &node : nodes)
    {
      for (auto &leave : node->getLeaves())
      {
        leaveNodes.push_back(leave);
      }
    }
    leaveNodesPerFile.push_back(std::move(leaveNodes));
  }
  std::cout << lcs.size() << "|" << leaveNodesPerFile[0].size() << "|"
            << leaveNodesPerFile[1].size() << std::endl;
  MatchList matchList;
  for (size_t i { 0 }; i < leaveNodesPerFile[0].size(); ++i)
  {
    Match match;
    for (size_t j { 0 }; j < leaveNodesPerFile.size(); ++j)
    {
      match.push_back(leaveNodesPerFile[j][i]);
    }
    matchList.push_back(std::move(match));
  }
  return matchList;
}

MatchList intersection2(const std::vector<std::unique_ptr<Node>> &files,
                        Configuration                            &config)
{
  std::vector<std::vector<Node *>>   tokenTables {};
  std::vector<std::vector<LCSToken>> lcsTables;
  for (auto &file : files)
  {
    auto &tokenTable { file->getLeaves() };
    tokenTables.push_back(tokenTable);
    lcsTables.push_back(nodeTableToLCSTable(tokenTable));
  }

  auto      result { runLCSRecursively(lcsTables) };
  MatchList resultMatches { matchLCSToNodes(result[0], files) };
  // for (size_t i = 0; i < resultMatches.size(); ++i)
  // {
  //   std::cout << resultMatches[i].size() << " "
  //             << resultMatches[i][0]->getTsText() << " "
  //             << resultMatches[i][1]->getTsText() << std::endl;
  // }
  return resultMatches;
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
