#include "set_operations.hpp"
#include "node_types.hpp"
#include "utility.hpp"
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
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

std::map<size_t, std::string> hashToTokenMap;

using IndexWithTokenTable = std::pair<size_t, std::vector<Node *>>;

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

LCS lcs(const std::vector<LCSToken> &a, const std::vector<LCSToken> &b)
{
  if (a == b)
  {
    return { a };
  }

  if (a.empty() || b.empty())
  {
    return {};
  }

  std::map<size_t, size_t> tokenCount;
  for (const auto &token : a)
  {
    if (tokenCount.find(token) == tokenCount.end())
    {
      tokenCount[token] = 1;
    }
    tokenCount[token]++;
  }

  size_t                        n = a.size(), m = b.size();
  std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));

  for (size_t i = 1; i <= n; ++i)
  {
    for (size_t j = 1; j <= m; ++j)
    {
      if (a[i - 1] == b[j - 1])
      {
        size_t weight { 20ul - std::min(tokenCount[a[i - 1]], 19ul) };
        dp[i][j] = dp[i - 1][j - 1] + weight;
      }
      else
      {
        dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
      }
      // std::cout << std::setw(5) << std::setfill(' ') << dp[i][j] << " ";
    }
    // std::cout << std::endl;
  }

  std::stack<std::tuple<size_t, size_t, LCS>> st;
  st.push({ n, m, LCS() });

  while (!st.empty())
  {
    auto [i, j, current] = st.top();
    st.pop();

    if (i == 0 || j == 0)
    {
      if (!current.empty())
      {
        std::reverse(current.begin(), current.end());
        return current;
      }
      continue;
    }

    if (a[i - 1] == b[j - 1])
    {
      LCS newCurrent = current;
      newCurrent.push_back(a[i - 1]);
      st.push({ i - 1, j - 1, newCurrent });
    }
    else
    {
      if (dp[i - 1][j] == dp[i][j])
      {
        st.push({ i - 1, j, current });
      }
      if (dp[i][j - 1] == dp[i][j])
      {
        st.push({ i, j - 1, current });
      }
    }
  }
  return {};
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
  hashToTokenMap.insert({ token, node->getTsText() });
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

LCS runLCSRecursively(std::vector<std::vector<LCSToken>> lcsTables)
{
  if (lcsTables.empty())
  {
    return {};
  }

  std::vector<std::vector<LCSToken>> currentResult { lcsTables };
  // Remove duplicate LCS tables
  std::sort(currentResult.begin(), currentResult.end());
  auto it = std::unique(currentResult.begin(), currentResult.end());
  currentResult.erase(it, currentResult.end());
  std::cout << "Running LCS recursively with " << currentResult.size()
            << " LCS tables\n";

  if (currentResult.size() == 1)
  {
    return currentResult[0];
  }

  std::vector<std::vector<LCSToken>> newResult {};
  for (size_t i = 0; i < currentResult.size() - 1; ++i)
  {
    auto result { lcs(currentResult[i], currentResult[i + 1]) };
    newResult.push_back(result);
  }

  if (newResult.size() > 1)
  {
    return runLCSRecursively(newResult);
  }
  else
  {
    return newResult.size() > 0 ? newResult[0] : std::vector<LCSToken> {};
  }
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
        // std::cout << "Current file index " << fileIndex << " and mapping
        // Index "
        //           << mappingIndex
        //           << " Token: " << currentMappingOption.node->getTsText()
        //           << std::endl;
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
    auto                subtreeLeaves { subtree->getLeafs() };
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

[[nodiscard]] std::vector<Node *> matchLCSWithTree(LCS &lcs, Node *&file)
{
  // check if lcs and file are the same
  auto fileTokens { file->getLeafs() };
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
    results.push_back(matchLCSWithTree(lcs, file));
  }
  return results;
}

LCS runLCSRecursively(std::vector<Node *> &files)
{
  std::vector<std::vector<Node *>>   tokenTables {};
  std::vector<std::vector<LCSToken>> lcsTables;
  for (auto &file : files)
  {
    auto &tokenTable { file->getLeafs() };
    tokenTables.push_back(tokenTable);
    lcsTables.push_back(nodeTableToLCSTable(tokenTable));
  }
  return runLCSRecursively(lcsTables);
}

std::pair<MatchList, LCS> intersection(std::vector<Node *> &files,
                                       Configuration       &config)
{
  auto result { runLCSRecursively(files) };
  if (result.empty())
  {
    return { {}, {} };
  }
  MatchList resultMatches { matchLCSWithTrees(result, files) };
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

  return { matches, result };
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

  auto leftSideLCS = runLCSRecursively(leftFilesRaw);
  std::cout << leftSideLCS.size() << " LCS tokens found for left side files\n";

  if (leftSideLCS.empty())
  {
    DifferenceResult dr {};
    for (auto &f : leftFiles)
    {
      dr.result.push_back(FileDifferenceResult {});
    }
    return dr;
  }

  std::set<std::vector<LCSToken>> leftSideUniqueTokenTables {};
  for (auto leftSideRawPointer : leftFilesRaw)
  {
    if (leftSideRawPointer->getTsText() == "")
    {
      continue; // skip empty files
    }
    auto &leftSideLeaves { leftSideRawPointer->getLeafs() };
    auto  lcsTable { nodeTableToLCSTable(leftSideLeaves) };
    leftSideUniqueTokenTables.insert(lcsTable);
  }
  std::cout << leftSideUniqueTokenTables.size()
            << " unique left side token tables found\n";

  std::set<std::vector<LCSToken>> rightSideUniqueTokenTables {};
  for (auto &rightSideFile : rightFiles)
  {
    if (rightSideFile->getTsText() == "")
    {
      continue; // skip empty files
    }
    auto &rightSideLeaves { rightSideFile->getLeafs() };
    auto  lcsTable { nodeTableToLCSTable(rightSideLeaves) };
    rightSideUniqueTokenTables.insert(lcsTable);
  }
  std::cout << rightSideUniqueTokenTables.size()
            << " unique right side token tables found\n";

  LCS result {};
  for (auto rightSideTokenTable : rightSideUniqueTokenTables)
  {
    auto subtractionLcs = lcs(nodeTableToLCSTable(leftFilesRaw[0]->getLeafs()),
                              rightSideTokenTable);
    if (subtractionLcs.size() > result.size())
    {
      result = subtractionLcs;
    }
  }

  auto matchListLeft { std::vector<std::vector<Node *>> {
      matchLCSWithTree(leftSideLCS, leftFilesRaw[0]) } };

  MatchList matchListRight {};
  if (!result.empty())
  {
    matchListRight.push_back(matchLCSWithTree(result, leftFilesRaw[0]));
  }

  DifferenceResult differenceResult {};
  for (size_t i { 0 }; i < matchListLeft.size(); ++i)
  {
    FileDifferenceResult fdr {};
    fdr.intersection = matchListLeft[i];
    fdr.subtraction  = (i < matchListRight.size() ? matchListRight[i]
                                                  : std::vector<Node *> {});
    differenceResult.result.push_back(fdr);
  }
  return differenceResult;
}
