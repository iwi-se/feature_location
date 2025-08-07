#include "set_operations.hpp"

std::map<size_t, std::string> hashToTokenMap;

template<> const LCSToken &getTokenRepresentation(Node *n)
{
  return n->getSubtreeHash();
}

template<> const std::string &getTokenRepresentation(Node *n)
{
  return n->getTsText();
}

size_t getTokenFromToken(std::pair<size_t, std::set<size_t>> lcsTokenType)
{
  return lcsTokenType.first;
}

template<>
void modifyNode(Node *node, std::pair<LCSToken, std::set<size_t>> lcsTokenType)
{
  node->setFeatureAFfiliations(lcsTokenType.second);
}

template std::vector<Node *> matchLCSWithTree<LCSToken>(LCS<LCSToken> &lcs,
                                                        Node         *&file);
template std::vector<Node *>
    matchLCSWithTree<std::string>(LCS<std::string> &lcs, Node *&file);

LCSToken nodeToLCSToken(Node *node, size_t index)
{
  LCSToken token { node->getSubtreeHash() };
  hashToTokenMap.insert({ token, node->getTsText() });
  return token;
}

MatchList matchLCSWithTrees(LCS<LCSToken> &lcs, std::vector<Node *> &files)
{
  std::vector<std::vector<Node *>> results;
  for (auto &file : files)
  {
    results.push_back(matchLCSWithTree<LCSToken>(lcs, file));
  }
  return results;
}

LCS<LCSToken> runLCSRecursively(std::vector<Node *> &files)
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

// bool operator< (const LCS &a, const LCS &b)
// {
//   return std::lexicographical_compare(a.begin(), a.end(), b.begin(),
//   b.end());
// }

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

bool checkTokenMappingStillPossible(
    const size_t                           &fileTokenIndex,
    const size_t                           &globalFileTokenIndex,
    const size_t                           &fileTokensAmount,
    std::vector<std::vector<MappingEntry>> &mapping,
    size_t                                  mappingIndex)
{
  if ((globalFileTokenIndex >= fileTokenIndex
       && globalFileTokenIndex != UINT_MAX))
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
    std::cout << "------------------------------";
    for (auto token : leftSideLeaves)
    {
      std::cout << token->getTsText() << std::endl;
      ;
    }
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

  std::set<size_t> indicesAfterSubtraction {};
  size_t           i { 0 };
  for (auto leftSideTokenTable : leftSideUniqueTokenTables)
  {
    std::set<size_t> indicesPerLeftSideTokenTable {};
    for (size_t i { 0 }; i < leftSideLCS.size(); ++i)
    {
      indicesPerLeftSideTokenTable.insert(i);
    }
    for (auto rightSideTokenTable : rightSideUniqueTokenTables)
    {
      auto subtractionLcs = lcs(leftSideTokenTable, rightSideTokenTable).lcs;
      auto subtractionFromIntersectionLcs = lcs(leftSideLCS, subtractionLcs);

      for (size_t i {}; i < subtractionFromIntersectionLcs.lcs.size(); ++i)
      {
        // std::cout << subtractionFromIntersectionLcs.leftIndices[i] << " "
        //           << hashToTokenMap[subtractionFromIntersectionLcs.lcs[i]]
        //           << std::endl;
      }
      for (auto index : subtractionFromIntersectionLcs.leftIndices)
      {
        indicesPerLeftSideTokenTable.erase(index);
      }
      if (indicesPerLeftSideTokenTable.empty())
      {
        break; // no more indices left, break out of the loop
      }
    }
    for (auto index : indicesPerLeftSideTokenTable)
    {
      indicesAfterSubtraction.insert(index);
    }
    std::cout << "Subtraction from left side table " << i++ << "/"
              << leftSideUniqueTokenTables.size() << " done" << std::endl;
  }

  LCS<LCSToken>       finalLCS {};
  std::vector<size_t> indicesAfterSubtractionVec(
      indicesAfterSubtraction.begin(), indicesAfterSubtraction.end());
  std::sort(indicesAfterSubtractionVec.begin(),
            indicesAfterSubtractionVec.end());
  for (auto index : indicesAfterSubtractionVec)
  {
    finalLCS.push_back(leftSideLCS[index]);

    // std::cout << index << " " << hashToTokenMap[leftSideLCS[index]]
    //           << std::endl;
  }

  std::cout << "\n------------------------\n";

  auto matchListLeft { std::vector<std::vector<Node *>> {
      matchLCSWithTree<LCSToken>(finalLCS, leftFilesRaw[0]) } };

  MatchList matchListRight {};
  // if (!result.empty())
  // {
  //   matchListRight.push_back(matchLCSWithTree(result, leftFilesRaw[0]));
  // }

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

std::pair<MatchList, LCS<LCSToken>> intersection(std::vector<Node *> &files,
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
