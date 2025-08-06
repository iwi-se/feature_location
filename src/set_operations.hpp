#ifndef SET_OPERATIONS_HPP
#define SET_OPERATIONS_HPP

#include "configuration.hpp"
#include "tree.hpp"
#include "utility.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using IndexWithTokenTable = std::pair<size_t, std::vector<Node *>>;

using Match = std::vector<Node *>;

using MatchList = std::vector<Match>;

using MatchesPerFile = std::vector<Node *>;

template<typename T> using LCS = std::vector<T>;

template<typename T> struct LCSResult
{
    LCS<T>              lcs;
    std::vector<size_t> leftIndices;
    std::vector<size_t> rightIndices;
};

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

template<typename T> using LCS = std::vector<T>;

struct MappingEntry
{
    Node  *node;
    size_t fileIndex;
    size_t weight;
};

using LCSToken = size_t;

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

template<typename A>
std::vector<LCSToken> nodeTableToLCSTable(std::vector<A> &nodeTable)
{
  LCS<LCSToken> lcs;
  for (size_t i = 0; i < nodeTable.size(); ++i)
  {
    lcs.push_back(nodeToLCSToken(nodeTable[i], i));
  }
  return lcs;
}

bool checkTokenMappingStillPossible(
    const size_t                           &fileTokenIndex,
    const size_t                           &globalFileTokenIndex,
    const size_t                           &fileTokensAmount,
    std::vector<std::vector<MappingEntry>> &mapping,
    size_t                                  mappingIndex);

template<typename T>
void calculateMappingWeights(Node                                  *&root,
                             std::vector<std::vector<MappingEntry>> &mapping,
                             const LCS<T>                           &lcs)
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
    auto           subtreeLeaves { subtree->getLeafs() };
    std::vector<T> subtreeTokens {};
    for (auto &leaf : subtreeLeaves)
    {
      subtreeTokens.push_back(getTokenRepresentation<T>(leaf));
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

size_t calculateCommonAncestorProximity(Node *node1, Node *node2);

template<typename T> const T &getTokenRepresentation(Node *)
{
  std::cerr << "Error: getTokenRepresentation not implemented for type "
            << typeid(T).name() << std::endl;
  exit(1);
}

template<typename T> T getTokenFromToken(T lcsTokenType)
{
  return lcsTokenType; // Default implementation for types that can be
                       // directly returned
}

size_t getTokenFromToken(std::pair<size_t, std::set<size_t>> lcsTokenType);

template<typename T> void modifyNode(Node *node, T lcsTokenType)
{
  // Do nothing by default
}

template<typename T, typename U>
[[nodiscard]] std::vector<Node *> matchLCSWithTree(LCS<U> &lcs, Node *&file)
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
    auto lcsToken { getTokenFromToken(lcs[i]) };
    for (size_t j { 0 }; j < fileTokens.size(); ++j)
    {
      if (getTokenRepresentation<T>(fileTokens[j]) == lcsToken)
      {
        // NOTE: Do not change order, checkTokenMappingStillPossible relies on
        // order for performance reasons
        mapping[i].push_back({ fileTokens[j], j, 0 });
      }
    }
  }

  LCS<T> lcsT {};
  for (auto token : lcs)
  {
    lcsT.push_back(getTokenFromToken(token));
  }
  calculateMappingWeights(file, mapping, lcsT);

  // Iterate through the mapping and always select the Node with the highest
  // weight
  std::vector<Node *> result {};
  size_t              globalFileTokenIndex { 0 };
  for (size_t i { 0 }; i < mapping.size(); ++i)
  {
    auto                     &tokenMapping { mapping[i] };
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
        size_t dist { 0 };
        if (!result.empty())
        {
          dist = calculateCommonAncestorProximity(*(result.end() - 1),
                                                  mappingEntry.node);
        }
        if (dist < currentLowestDistance)
        {
          currentLowestDistance        = dist;
          currentLowestDistanceMapping = mappingEntry;
        }
      }
      modifyNode(currentLowestDistanceMapping.node, lcs[i]);
      result.push_back(currentLowestDistanceMapping.node);
      globalFileTokenIndex = currentLowestDistanceMapping.fileIndex;
    }
    else
    {
      modifyNode(currentHighestMapping[0].node, lcs[i]);
      result.push_back(currentHighestMapping[0].node);
      globalFileTokenIndex = currentHighestMapping[0].fileIndex;
    }
  }
  printDebug(false, "\n\n");
  return result;
}

MatchesPerFile extractMatchesPerFile(const MatchList &matches,
                                     const int       &index);

MatchList intersection(const std::vector<Node *> &nodes1,
                       Configuration             &config);

DifferenceResult
    difference(const std::vector<std::unique_ptr<Node>> &leftFiles,
               const std::vector<std::unique_ptr<Node>> &rightFiles,
               Configuration                            &config);

template<typename T>
LCSResult<T> lcs(const std::vector<T> &a, const std::vector<T> &b)

{
  if (a == b)
  {
    return { a };
  }

  if (a.empty() || b.empty())
  {
    return {};
  }

  std::map<T, size_t> tokenCount;
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

  std::stack<std::tuple<size_t, size_t, LCSResult<T>>> st;
  st.push({ n, m, LCSResult<T>() });

  while (!st.empty())
  {
    auto [i, j, currentResult] = st.top();
    st.pop();
    LCS<T> current { currentResult.lcs };

    if (i == 0 || j == 0)
    {
      if (!current.empty())
      {
        std::reverse(currentResult.lcs.begin(), currentResult.lcs.end());
        std::reverse(currentResult.leftIndices.begin(),
                     currentResult.leftIndices.end());
        std::reverse(currentResult.rightIndices.begin(),
                     currentResult.rightIndices.end());
        return currentResult;
      }
      continue;
    }

    if (a[i - 1] == b[j - 1])
    {
      LCSResult newCurrent = currentResult;
      newCurrent.lcs.push_back(a[i - 1]);
      newCurrent.leftIndices.push_back(i - 1);
      newCurrent.rightIndices.push_back(j - 1);
      st.push({ i - 1, j - 1, newCurrent });
    }
    else
    {
      if (dp[i - 1][j] == dp[i][j])
      {
        st.push({ i - 1, j, currentResult });
      }
      if (dp[i][j - 1] == dp[i][j])
      {
        st.push({ i, j - 1, currentResult });
      }
    }
  }
  return {};
}

template<typename T>
LCS<T> runLCSRecursively(std::vector<std::vector<T>> lcsTables)
{
  if (lcsTables.empty())
  {
    return {};
  }

  std::set<std::vector<T>> currentResultSet {};
  for (const auto &lcsTable : lcsTables)
  {
    currentResultSet.insert(lcsTable);
  }
  std::vector<std::vector<T>> currentResult(currentResultSet.begin(),
                                            currentResultSet.end());
  // Remove duplicate LCS tables
  std::cout << "Running LCS recursively with " << currentResult.size()
            << " LCS tables\n";

  if (currentResult.size() == 1)
  {
    return currentResult[0];
  }

  std::vector<std::vector<T>> newResult {};
  for (size_t i = 0; i < currentResult.size() - 1; ++i)
  {
    auto result { lcs(currentResult[i], currentResult[i + 1]) };
    newResult.push_back(result.lcs);
  }

  if (newResult.size() > 1)
  {
    return runLCSRecursively(newResult);
  }
  else
  {
    return newResult.size() > 0 ? newResult[0] : std::vector<T> {};
  }
}

#endif
