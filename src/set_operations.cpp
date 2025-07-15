#include "set_operations.hpp"
#include "node_types.hpp"
#include "utility.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <ranges>
#include <span>
#include <stack>
#include <stdexcept>
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

using LCSToken = size_t;

using LCS = std::vector<LCSToken>;

bool operator< (const LCS &a, const LCS &b)
{
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

std::vector<LCS> allLCS(const std::vector<LCSToken> &a,
                        const std::vector<LCSToken> &b)
{
  if (a == b)
  {
    return { a };
  }

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
    if (!resultSet.empty())
    {
      return;
    }
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
  LCSToken token { node->getSubtreeHash() };
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
    for (size_t i = 0; i < currentResult.size() - 1; ++i)
    {
      auto result { allLCS(currentResult[i], currentResult[i + 1]) };
      newResult.push_back(result[0]);
    }
    currentResult = std::move(newResult);
  }
  return currentResult;
}

size_t calculateCommonAncestorProximity(Node *node1, Node *node2)
{
  size_t distance { 0 };
  while (node1 != nullptr)
  {
    node1 = node1->getParent();
    size_t innerDistance { 0 };
    auto   tempNode2 { node2 };
    while (tempNode2 != nullptr)
    {
      tempNode2 = tempNode2->getParent();
      if (node1 == tempNode2)
      {
        auto numberOfAncestors = node1->getAncestors().size();
        // auto temp { (distance + innerDistance < numberOfAncestors
        //                  ? 0
        //                  : distance + innerDistance - numberOfAncestors) };
        // return temp;
        //
        // return distance + innerDistance; // return the minimum distance
        //
        return innerDistance;
      }
      innerDistance++;
    }
    distance++;
  }
  throw std::logic_error("Nodes have no common ancestor");
}

size_t calculateWeight(size_t                            i,
                       size_t                            j,
                       std::vector<std::vector<size_t>> &matrix,
                       std::vector<Node *>              &tokenTable,
                       LCS                              &lcs)
{
  auto  &token { tokenTable[j - 1] };
  size_t result { 0 };
  i--; // go one row up
  auto jBackwards { j };
  jBackwards--; // go one column left
  while (j > 0)
  {
    if (i < 1 || jBackwards < 1)
    {
      break;
    }
    if (tokenTable[jBackwards - 1]->getSubtreeHash() == lcs[i - 1])
    {
      size_t base { matrix[i][jBackwards] };
      auto   cap { calculateCommonAncestorProximity(tokenTable[jBackwards - 1],
                                                  token) };

      auto result_temp = base + 1 + (20ul > cap ? (20ul - cap) : 0ul);
      if (result_temp > result)
      {
        result = result_temp;
      }
    }
    jBackwards--;
  }

  // also go one row down
  i = i + 2;
  j++;
  size_t highestValue { 0 };
  while (j > 0)
  {
    if (tokenTable.size() < j || i >= matrix.size())
    {
      break;
    }
    if (tokenTable[j - 1]->getSubtreeHash() == lcs[i - 1])
    {
      auto   cap { calculateCommonAncestorProximity(token, tokenTable[j - 1]) };
      size_t highestValue_temp { 20ul > cap ? 20ul - cap : 0ul };
      if (highestValue_temp > highestValue)
      {
        highestValue = highestValue_temp;
      }
      break;
    }
    j++;
  }
  result += highestValue;
  return result;
}

[[nodiscard]] std::vector<Node *>
    matchLCSWithTree(LCS &lcs, const std::unique_ptr<Node> &file)
{
  auto                            &tokenTable { file->getLeaves() };
  std::vector<std::vector<size_t>> matrix(
      lcs.size() + 1, std::vector<size_t>(tokenTable.size() + 1, 0));

  // print tokentable size and lcs size
  printDebug(true,
             "TokenTable size: ",
             tokenTable.size(),
             ", LCS size: ",
             lcs.size(),
             "\n");
  for (size_t i = 0; i < lcs.size(); ++i)
  {
    auto &token { lcs[i] };
    printDebug(true, std::setw(30), token);
    for (size_t j = 0; j < tokenTable.size(); ++j)
    {
      if (tokenTable[j]->getSubtreeHash() == token)
      {
        size_t weight { calculateWeight(
            i + 1, j + 1, matrix, tokenTable, lcs) };
        matrix[i + 1][j + 1] = weight;
        // print weight always with the same number of places, e.g. 5
        printDebug(true, std::setw(5), weight, " ");
      }
      else
      {
        matrix[i + 1][j + 1] = matrix[i + 1][j];
        printDebug(true, std::setw(5), matrix[i + 1][j], " ");
      }
    }
    printDebug(true, "\n");
  }

  std::vector<Node *> result;
  size_t              i { lcs.size() };
  size_t              max_j { tokenTable.size() };
  while (i > 0)
  {
    size_t j { max_j };
    size_t current_best_j { max_j };
    size_t current_best_weight { 0 };

    while (j > 0)
    {
      if (lcs[i - 1] == tokenTable[j - 1]->getSubtreeHash()
          && matrix[i][j] > current_best_weight)
      {
        current_best_j      = j;
        current_best_weight = matrix[i][j];
      }
      j--;
    }

    auto tokenNode { tokenTable[current_best_j - 1] };
    tokenNode->structuralSimilarity = current_best_weight;
    result.insert(result.begin(), tokenNode);
    --i;
    max_j = current_best_j - 1;
  }

  return result;
}

struct MappingEntry
{
    Node  *node;
    size_t fileIndex;
    size_t weight;
};

bool checkTokenMappingStillPossible(
    const size_t                           &fileTokenIndex,
    const size_t                           &globalFileTokenIndex,
    const size_t                           &fileTokensAmount,
    std::vector<std::vector<MappingEntry>> &mapping,
    size_t                                  mappingIndex)
{
  if ((globalFileTokenIndex >= fileTokenIndex && globalFileTokenIndex != 0))
  {
    return false;
  }

  if (fileTokenIndex - globalFileTokenIndex
      == 1) // if file token is the direct next token, it must be possible
  {
    return true;
  }

  // if there are less tokens in the file than in the lcs, it is not possible
  if (fileTokensAmount - fileTokenIndex < mapping.size() - mappingIndex)
  {
    return false;
  }

  mappingIndex++;
  size_t currentIndex { fileTokenIndex };
  while (mappingIndex < mapping.size())
  {
    const auto &currentMappingOptions { mapping[mappingIndex] };
    bool        foundIndex { false };
    for (const auto &currentMappingOption : currentMappingOptions)
    {
      const size_t &fileIndex { currentMappingOption.fileIndex };
      if (fileIndex > currentIndex)
      {
        currentIndex = fileIndex;
        foundIndex   = true;
        break;
      }
    }
    if (!foundIndex)
    {
      return false; // no valid mapping found
    }
    ++mappingIndex;
  }
  return true;
}

void calculateMappingWeights(Node                                  *&root,
                             std::vector<std::vector<MappingEntry>> &mapping,
                             const LCS                              &lcs)
{
  // Iterate over subtrees. If all tokens of subtree are in order and directly
  // besides each other in lcs, set the weight to subtrees size, if higher
  // than current weight
  std::stack<Node *> subtrees {};
  subtrees.push(root);
  while (!subtrees.empty())
  {
    auto subtree { subtrees.top() };
    subtrees.pop();
    auto                subtreeLeaves { subtree->getLeaves() };
    std::vector<size_t> subtreeTokens {};
    for (auto &leaf : subtreeLeaves)
    {
      subtreeTokens.push_back(leaf->getSubtreeHash());
    }

    auto it { std::search(
        lcs.begin(), lcs.end(), subtreeTokens.begin(), subtreeTokens.end()) };

    if (it == lcs.end())
    {
      // No match found, continue with next subtree
      for (auto &child : subtree->getChildren())
      {
        subtrees.push(child.get());
      }
      continue;
    }

    while (it != lcs.end())
    {
      auto first_index { it - lcs.begin() };
      for (auto index { first_index };
           index < (first_index + subtreeTokens.size());
           index++)
      {
        for (auto &tuple : mapping[index])
        {
          if (tuple.node == subtreeLeaves[index - first_index])
          {
            if (tuple.weight < subtreeLeaves.size())
            {
              tuple.weight = subtreeLeaves.size();
            }
            break;
          }
        }
      }
      it = { std::search(
          it + 1, lcs.end(), subtreeTokens.begin(), subtreeTokens.end()) };
    }
  }
}

[[nodiscard]] std::vector<Node *> matchLCSWithTree2(LCS &lcs, Node *&file)
{
  // check if lcs and file are the same
  auto fileTokens { file->getLeaves() };
  if (fileTokens.size() == lcs.size())
  {
    // Must be equal, because lcs was done with the same file
    return fileTokens;
  }

  // get all Subtress larger than 1
  auto subtrees = file->getPointerToEveryNode();
  for (auto it { subtrees.begin() }; it != subtrees.end();)
  {
    if ((*it)->isLeaf())
    {
      it = subtrees.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // build a mapping from LCS tokens to file tokens
  std::vector<std::vector<MappingEntry>> mapping(lcs.size());
  for (size_t i { 0 }; i < lcs.size(); ++i)
  {
    auto &lcsToken { lcs[i] };
    for (size_t j { 0 }; j < fileTokens.size(); ++j)
    {
      if (fileTokens[j]->getSubtreeHash() == lcsToken)
      {
        // NOTE: Do not change order, checkTokenMappingStillPossible relies on
        // order for performance reasons
        mapping[i].push_back({ fileTokens[j], j, 0 });
      }
    }
  }

  calculateMappingWeights(file, mapping, lcs);

  // Iterate through the mapping and always select the Node with the highest
  // weight
  std::vector<Node *> result {};
  size_t              globalFileTokenIndex { 0 };
  for (auto &tokenMapping : mapping)
  {
    size_t                    currentHighestWeight { 0 };
    std::vector<MappingEntry> currentHighestMapping {};
    for (auto &possibleFileTokenWithWeight : tokenMapping)
    {
      auto &node { possibleFileTokenWithWeight.node };
      auto &fileTokenIndex { possibleFileTokenWithWeight.fileIndex };
      auto &weight { possibleFileTokenWithWeight.weight };
      printDebug(false,
                 "Token: ",
                 node->getTsText(),
                 ", Weight: ",
                 weight,
                 ", Index: ",
                 fileTokenIndex,
                 "\n");
      if (weight >= currentHighestWeight
          && (checkTokenMappingStillPossible(
              fileTokenIndex,
              globalFileTokenIndex,
              fileTokens.size(),
              mapping,
              result.size()))) // tokenIndex > fileTokenIndex || fileTokenIndex
                               // == 0))
      {
        if (weight == currentHighestWeight)
        {
          currentHighestMapping.push_back(possibleFileTokenWithWeight);
        }
        else
        {
          currentHighestMapping.clear();
          currentHighestMapping.push_back(possibleFileTokenWithWeight);
          currentHighestWeight = weight;
        }
      }
    }
    printDebug(false,
               currentHighestMapping.size(),
               " nodes with highest weight: ",
               currentHighestWeight,
               "\n");
    if (currentHighestMapping.size() > 1)
    {
      size_t       currentLowestDistance { std::numeric_limits<size_t>::max() };
      MappingEntry currentLowestDistanceMapping {};
      for (auto &mappingEntry : currentHighestMapping)
      {
        auto dist { calculateCommonAncestorProximity(*(result.end() - 1),
                                                     mappingEntry.node) };
        if (dist < currentLowestDistance)
        {
          currentLowestDistance        = dist;
          currentLowestDistanceMapping = mappingEntry;
        }
      }
      result.push_back(currentLowestDistanceMapping.node);
      globalFileTokenIndex = currentLowestDistanceMapping.fileIndex;
    }
    else
    {
      result.push_back(currentHighestMapping[0].node);
      globalFileTokenIndex = currentHighestMapping[0].fileIndex;
    }
  }
  printDebug(false, "\n\n");
  return result;
}

MatchList matchLCSWithTrees(LCS &lcs, std::vector<Node *> &files)
{
  std::vector<std::vector<Node *>> results;
  for (auto &file : files)
  {
    results.push_back(matchLCSWithTree2(lcs, file));
  }
  return results;
}

std::vector<LCS> runLCSRecursively(std::vector<Node *> &files)
{
  std::vector<std::vector<Node *>>   tokenTables {};
  std::vector<std::vector<LCSToken>> lcsTables;
  for (auto &file : files)
  {
    auto &tokenTable { file->getLeaves() };
    tokenTables.push_back(tokenTable);
    lcsTables.push_back(nodeTableToLCSTable(tokenTable));
  }
  return runLCSRecursively(lcsTables);
}

std::pair<MatchList, LCS> intersection2(std::vector<Node *> &files,
                                        Configuration       &config)
{
  auto      result { runLCSRecursively(files) };
  MatchList resultMatches { matchLCSWithTrees(result[0], files) };
  // for (size_t i = 0; i < resultMatches.size(); ++i)
  // {
  //   std::cout << resultMatches[i].size() << " "
  //             << resultMatches[i][0]->getTsText() << " "
  //             << resultMatches[i][1]->getTsText() << std::endl;
  // }
  MatchList matches;
  for (size_t i = 0; i < resultMatches[0].size(); ++i)
  {
    Match match;
    for (size_t j = 0; j < resultMatches.size(); ++j)
    {
      match.push_back(resultMatches[j][i]);
    }
    matches.push_back(match);
  }

  return { matches, result[0] };
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

  std::vector<Node *> leftFilesRaw {};
  for (auto &node : leftFiles)
  {
    leftFilesRaw.push_back(node.get());
  }

  auto  intersectionResult = intersection2(leftFilesRaw, config);
  auto &leftSideIntersection { intersectionResult.first };
  auto &leftSideLCS { intersectionResult.second };

  std::vector<LCS> subtractionLCSByFiles {};
  for (size_t index = 0; index < leftFiles.size(); index++)
  {
    std::vector<LCS> allSingleLCSs {};
    for (const auto &rightFile : rightFiles)
    {
      TreeIndex           rightSideIndex(rightFile.get());
      std::vector<Node *> param { leftFiles[index].get(), rightFile.get() };
      auto                subtractionLcs = runLCSRecursively(param);
      auto                intersectionLcsMinusSubtraction { allLCS(leftSideLCS,
                                                    subtractionLcs[0]) };
      allSingleLCSs.push_back(intersectionLcsMinusSubtraction[0]);
    }
    auto result { runLCSRecursively(allSingleLCSs) };
    subtractionLCSByFiles.push_back(result[0]);
  }

  // sort subtractionLCSByFiles by lcs size
  std::sort(subtractionLCSByFiles.begin(),
            subtractionLCSByFiles.end(),
            [](const LCS &a, const LCS &b) { return a.size() < b.size(); });

  auto matchListLeft { matchLCSWithTrees(leftSideLCS, leftFilesRaw) };
  auto matchListRight { matchLCSWithTrees(subtractionLCSByFiles[0],
                                          leftFilesRaw) };
  DifferenceResult differenceResult {};
  for (size_t i { 0 }; i < matchListLeft.size(); ++i)
  {
    FileDifferenceResult fdr {};
    fdr.intersection = matchListLeft[i];
    fdr.subtraction  = matchListRight[i];
    differenceResult.result.push_back(fdr);
  }
  return differenceResult;
}
