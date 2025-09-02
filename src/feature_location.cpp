#include "feature_location.hpp"
#include "configuration.hpp"
#include "parser.hpp"
#include "render.hpp"
#include "set_operations.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

std::vector<std::pair<Node *, std::string>>
    addColorsToNodes(std::vector<Node *> nodes)
{
  std::vector<std::pair<Node *, std::string>> nodesWithColors {};
  std::stack<std::string>                     colorStack {
                        { "#FFFACD",
                         "#D0F0C0", "#FFECB3",
                         "#CCE5FF", "#FFDDE1",
                         "#E0BBE4", "#F0E68C",
                         "#D5F4E6", "#FAD6A5",
                         "#E6E6FA" }
  };
  std::map<std::set<size_t>, std::string> featureColorMap {};
  for (auto &node : nodes)
  {
    if (!featureColorMap.contains(node->getFeatureAffiliations()))
    {
      featureColorMap[node->getFeatureAffiliations()] = colorStack.top();
      colorStack.pop();
    }
    nodesWithColors.push_back(
        { node, featureColorMap[node->getFeatureAffiliations()] });
  }
  return nodesWithColors;
}

std::map<std::string, std::unique_ptr<Node>>
    parseFiles(const NamePathMappings &npms)
{
  std::map<std::string, std::unique_ptr<Node>> nameASTMap;
  for (auto &mapping : npms)
  {
    nameASTMap[mapping.first] = parseFile(mapping.second.paths[0], "cpp");
  }
  return nameASTMap;
}

std::vector<std::string> getStringTokens(Node *n)
{
  std::vector<std::string> result;
  for (auto &l : n->getLeafs())
  {
    result.push_back(l->getTsText());
  }
  return result;
}

double ancestorSimilarity(Node *n1, Node *n2)
{
  if (n1 == nullptr || n2 == nullptr || n1->getParent() == nullptr
      || n2->getParent() == nullptr)
  {
    return 0;
  }
  double result {};
  while (n1->getParent()->getTag() == n2->getParent()->getTag())
  {
    n1 = n1->getParent();
    n2 = n2->getParent();
    if (n1->getSubtreeHash() == n2->getSubtreeHash())
    {
      result += 10;
    }
    else
    {
      auto n1Leaves { getStringTokens(n1) };
      auto n2Leaves { getStringTokens(n2) };
      auto lcsResult { lcs(n1Leaves, n2Leaves) };
      result += (10.0 * static_cast<double>(lcsResult.lcs.size())
                 / std::max(n1Leaves.size(), n2Leaves.size()));
    }

    if (n1->getParent() == nullptr || n2->getParent() == nullptr)
    {
      break;
    }
  }
  return result;
}

LCSResult<Node *> treeLcs(const std::vector<Node *> &a,
                          const std::vector<Node *> &b)
{
  if (a == b)
  {
    return { a };
  }

  if (a.empty() || b.empty())
  {
    return {};
  }

  std::map<size_t, size_t> hashCount;
  for (const auto &token : a)
  {
    if (hashCount.find(token->getSubtreeHash()) == hashCount.end())
    {
      hashCount[token->getSubtreeHash()] = 1;
    }
    hashCount[token->getSubtreeHash()]++;
  }

  size_t                           n = a.size(), m = b.size();
  std::vector<std::vector<double>> dp(n + 1, std::vector<double>(m + 1, 0));

  for (size_t i = 1; i <= n; ++i)
  {
    for (size_t j = 1; j <= m; ++j)
    {
      double option1 { 0 }, option2 { 0 };
      if (a[i - 1]->getSubtreeHash() == b[j - 1]->getSubtreeHash())
      {
        double weight {
          ancestorSimilarity(a[i - 1], b[j - 1]) + 1
          /*- std::min(hashCount[a[i - 1]->getSubtreeHash()],
                     4ul)*/
        };
        option1 = dp[i - 1][j - 1] + weight;
      }
      option2  = std::max(dp[i - 1][j], dp[i][j - 1]);
      dp[i][j] = std::max(option1, option2);
      std::cout << std::setw(40) << std::setfill(' ') << a[i - 1]->getTsText()
                << ":" << b[j - 1]->getTsText() << ":" << std::setw(12)
                << std::setfill(' ') << dp[i][j] << " ";
    }
    std::cout << std::endl;
  }

  std::stack<std::tuple<size_t, size_t, LCSResult<Node *>>> st;
  st.push({ n, m, LCSResult<Node *>() });

  while (!st.empty())
  {
    auto [i, j, currentResult] = st.top();
    st.pop();
    LCS<Node *> current { currentResult.lcs };

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

    if (a[i - 1]->getSubtreeHash() == b[j - 1]->getSubtreeHash()
        && std::abs(dp[i - 1][j - 1] + ancestorSimilarity(a[i - 1], b[j - 1])
                    + 1 - dp[i][j])
               < 0.5)
    {
      LCSResult newCurrent = currentResult;
      a[i - 1]->weight     = dp[i][j];
      newCurrent.lcs.push_back(a[i - 1]);
      newCurrent.leftIndices.push_back(i - 1);
      newCurrent.rightIndices.push_back(j - 1);
      std::cout << "Selecting token " << a[i - 1]->getTsText()
                << " with added weight " << dp[i][j] - dp[i - 1][j - 1]
                << std::endl;
      st.push({ i - 1, j - 1, newCurrent });
    }
    else
    {
      if (dp[i - 1][j] >= dp[i][j - 1])
      {
        st.push({ i - 1, j, currentResult });
      }
      else
      {
        st.push({ i, j - 1, currentResult });
      }
    }
  }
  return {};
}

void test(std::map<std::string, std::unique_ptr<Node>> &parsedFiles,
          const Configuration                          &config)
{
  auto s1 { parsedFiles["S2"].get() };
  auto s2 { parsedFiles["S1"].get() };

  auto res { treeLcs(s1->getLeafs(), s2->getLeafs()) };

  std::vector<Node *> resLeavesOnly {};
  for (auto &node : res.lcs)
  {
    if (node->isLeaf())
    {
      resLeavesOnly.push_back(node);
    }
  }

  auto nodesWithColors { addColorsToNodes(resLeavesOnly) };
  auto rendered { renderFile("example/rl.hpp", config, nodesWithColors) };

  std::ofstream outFile("test.html");
  outFile << rendered;
  outFile.close();
}

void featureLocation(Configuration config)
{
  auto namePathMappings = config.getNamePathMappings();
  auto parsedFiles { parseFiles(namePathMappings) };
  test(parsedFiles, config);
}
