#include "feature_location.hpp"
#include "configuration.hpp"
#include "parser.hpp"
#include "render.hpp"
#include "set_operations.hpp"
#include "tree_diff.hpp"
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

struct DiffLine
{
    size_t           source_line;
    size_t           content;
    std::set<size_t> feature_affiliations;
};

struct DiffLineCompareWOSource
{
    DiffLine diffline;
};

// overload operator== for DiffLine to allow comparison
bool operator== (const DiffLine& lhs, const DiffLine& rhs)

{
  return lhs.source_line == rhs.source_line && lhs.content == rhs.content;
}

bool operator< (const DiffLine& lhs, const DiffLine& rhs)
{
  return lhs.source_line < rhs.source_line;
}

bool operator== (const DiffLineCompareWOSource& lhs,
                 const DiffLineCompareWOSource& rhs)
{
  return lhs.diffline.content == rhs.diffline.content;
}

bool operator< (const DiffLineCompareWOSource& lhs,
                const DiffLineCompareWOSource& rhs)
{
  return lhs.diffline.source_line == rhs.diffline.source_line;
}

struct DiffInfo
{
    std::string      source {};
    std::string      dest {};
    Node*            sourceAST {};
    Node*            destAST {};
    std::set<size_t> added_features {};   // bitmap
    std::set<size_t> removed_features {}; // bitmap
};

struct DiffResult
{
    std::vector<DiffLine> added_lines {};
    std::vector<DiffLine> removed_lines {};
    DiffInfo              info {};
    std::string           raw_diff_output {}; // For debugging purposes
};

void printDiffResult(DiffResult diffResult)
{
  std::cout << "Diff Info: " << diffResult.info.source << " -> "
            << diffResult.info.dest << std::endl;
  for (const auto& line : diffResult.added_lines)
  {
    std::cout << "Added: " << line.source_line << ": " << line.content
              << std::endl;
  }
  for (const auto& line : diffResult.removed_lines)
  {
    std::cout << "Removed: " << line.source_line << ": " << line.content
              << std::endl;
  }
}

std::vector<DiffResult>
    filterDiffResultsBySrcSystem(std::vector<DiffResult> diffResults,
                                 std::string             srcSystem)
{
  std::vector<DiffResult> filteredResults;
  for (const auto& result : diffResults)
  {
    if (result.info.source == srcSystem)
    {
      filteredResults.push_back(result);
    }
  }
  return filteredResults;
}

std::vector<DiffResult>
    filterDiffResultsByAddedFeatures(std::vector<DiffResult> diffResults,
                                     std::vector<size_t>     features)
{
  std::vector<DiffResult> filteredResults;
  for (const auto& result : diffResults)
  {
    bool hasAll { true };
    for (const auto& feature : features)
    {
      if (std::find(result.info.added_features.begin(),
                    result.info.added_features.end(),
                    feature)
          == result.info.added_features.end())
      {
        hasAll = false;
        break;
      }
    }
    if (hasAll)
    {
      filteredResults.push_back(result);
    }
  }
  return filteredResults;
}

std::vector<DiffResult>
    filterDiffResultsByNotAddedFeatures(std::vector<DiffResult> diffResults,
                                        std::vector<size_t>     features)
{
  std::vector<DiffResult> filteredResults;
  for (const auto& result : diffResults)
  {
    for (const auto& feature : features)
    {
      if (std::find(result.info.added_features.begin(),
                    result.info.added_features.end(),
                    feature)
          == result.info.added_features.end())
      {
        filteredResults.push_back(result);
      }
    }
  }
  return filteredResults;
}

std::string featuresToString(std::set<size_t> s)
{
  std::string result;
  for (auto& el : s)
  {
    result += std::to_string(el) + " ";
  }
  return result;
}

void parseDiffOutput(const std::string&     diffOutput,
                     std::vector<DiffLine>& removedLines,
                     std::set<size_t>       featureAffiliationsRemoved,
                     std::vector<DiffLine>& addedLines,
                     std::set<size_t>       featureAffiliationsAdded)
{
  std::istringstream stream(diffOutput);
  std::string        line;

  size_t srcLineNum = 0; // Will track position in original file
  size_t tgtLineNum = 0; // Position in new file

  bool inHunk = false;

  while (std::getline(stream, line))
  {
    if (line.rfind("@@", 0) == 0)
    {
      inHunk = true;
      std::smatch match;
      std::regex  hunkRegex(R"(@@ -(\d+),?\d* \+(\d+),?\d* @@)");
      if (std::regex_search(line, match, hunkRegex))
      {
        srcLineNum
            = std::stoul(match[1]); // 1-based line number in original file
        tgtLineNum = std::stoul(match[2]); // 1-based line number in new file
      }
    }
    else if (inHunk && !line.empty())
    {
      char        prefix     = line[0];
      std::string contentStr = line.substr(1);
      size_t      content    = std::stoul(contentStr);

      switch (prefix)
      {
        case ' ' :
          ++srcLineNum;
          ++tgtLineNum;
          break;

        case '-' :
          removedLines.push_back(
              { srcLineNum, content, featureAffiliationsRemoved });
          ++srcLineNum;
          break;

        case '+' :
          // Associate added line with the current source position (i.e., after
          // which original line it appears)
          addedLines.push_back(
              { srcLineNum, content, featureAffiliationsAdded });
          ++tgtLineNum;
          break;
      }
    }
  }
}

std::string execDiff(const std::string& file1, const std::string& file2)
{
  std::array<char, 128> buffer;
  std::string           result;

  std::string command = "git diff -u --no-index --diff-algorithm=minimal "
                        + file1 + " " + file2;

  // Open the command for reading.
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"),
                                                pclose);
  if (!pipe)
  {
    throw std::runtime_error("popen() failed!");
  }

  // Read the output a chunk at a time.
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
  {
    result += buffer.data();
  }

  return result;
}

std::vector<DiffResult> runDiffs(std::vector<DiffInfo> diffs)
{
  std::vector<DiffResult> results;
  for (auto diff : diffs)
  {
    DiffResult    result;
    auto          tempfileSourceName { diff.source + ".tokenhashes.tmp" };
    auto          tempfileDestName { diff.dest + ".tokenhashes.tmp" };
    std::ofstream tempfileSource { tempfileSourceName };
    std::ofstream tempfileDest { tempfileDestName };
    for (auto& node : diff.sourceAST->getLeafs())
    {
      tempfileSource << node->getSubtreeHash() << "\n";
    }
    for (auto& node : diff.destAST->getLeafs())
    {
      tempfileDest << node->getSubtreeHash() << "\n";
    }
    tempfileSource << std::flush;
    tempfileDest << std::flush;
    tempfileSource.close();
    tempfileDest.close();

    auto strResult { execDiff(tempfileSourceName, tempfileDestName) };
    parseDiffOutput(strResult,
                    result.removed_lines,
                    diff.removed_features,
                    result.added_lines,
                    diff.added_features);
    result.info            = diff;
    result.raw_diff_output = strResult;
    results.push_back(result);
  }
  return results;
}

std::vector<size_t> runDiffs2(std::vector<size_t> a, std::vector<size_t> b)
{
  auto          aFileName { "a.tokens.tmp" };
  auto          bFileName { "b.tokens.tmp" };
  std::ofstream aFile { aFileName };
  std::ofstream bFile { bFileName };
  for (auto& tok : a)
  {
    aFile << tok << "\n";
  }
  for (auto& tok : b)
  {
    bFile << tok << "\n";
  }
  aFile << std::flush;
  bFile << std::flush;
  aFile.close();
  bFile.close();

  auto strResult { execDiff(aFileName, bFileName) };

  std::vector<DiffLine> removedLines;
  std::set<size_t>      featureAffiliationsRemoved;
  std::vector<DiffLine> addedLines;
  std::set<size_t>      featureAffiliationsAdded;
  parseDiffOutput(strResult,
                  removedLines,
                  featureAffiliationsRemoved,
                  addedLines,
                  featureAffiliationsAdded);

  std::vector<size_t> result {};
  for (auto& line : addedLines)
  {
    result.push_back(line.content);
  }
  return result;
}

bool allSameSource(const std::vector<DiffResult>& diffResults)
{
  const auto& firstSource = diffResults[0].info.source;
  for (const auto& result : diffResults)
  {
    if (result.info.source != firstSource)
    {
      return false; // If any source differs, return false
    }
  }
  return true; // All sources are the same
}

std::vector<DiffLine>
    sameSourceIntersection(std::vector<std::vector<DiffLine>> linesPerDiff,
                           std::set<size_t> resultingFeatureAffiliations)
{
  std::vector<DiffLine> intersection;
  if (linesPerDiff.empty())
  {
    return intersection; // Return empty if no results
  }

  auto lcsResult { runLCSRecursively(linesPerDiff) };
  for (auto& line : lcsResult)
  {
    line.feature_affiliations
        = resultingFeatureAffiliations; // Set the feature affiliations
  }
  return lcsResult;
}

std::set<size_t> featureIntersection(std::vector<std::set<size_t>> featureSets)
{
  if (featureSets.empty())
  {
    return {}; // Return empty if no feature sets
  }
  if (featureSets.size() == 1)
  {
    return featureSets[0];
  }

  std::set<size_t> intersection = featureSets[0];
  for (size_t i = 1; i < featureSets.size(); ++i)
  {
    std::set<size_t> currentSet = featureSets[i];
    std::set<size_t> newIntersection;
    set_intersection(intersection.begin(),
                     intersection.end(),
                     currentSet.begin(),
                     currentSet.end(),
                     std::inserter(newIntersection, newIntersection.begin()));
    intersection = newIntersection;
  }

  return intersection;
}

std::vector<DiffLine>
    differentSourceIntersection(std::vector<std::vector<DiffLine>> linesPerDiff,
                                std::set<size_t> resultingFeatureAffiliations)
{
  std::vector<DiffLine> intersection;
  if (linesPerDiff.empty())
  {
    return intersection; // Return empty if no results
  }

  std::vector<std::vector<DiffLineCompareWOSource>> difflinesCompareWrapper {};
  for (const auto& diffLines : linesPerDiff)
  {
    std::vector<DiffLineCompareWOSource> difflines {};
    for (const auto& line : diffLines)
    {
      DiffLineCompareWOSource compareLine { line };
      difflines.push_back(compareLine);
    }
    difflinesCompareWrapper.push_back(difflines);
  }

  auto lcsResult { runLCSRecursively(difflinesCompareWrapper) };
  for (auto& line : lcsResult)
  {
    line.diffline.feature_affiliations
        = resultingFeatureAffiliations; // Set the feature affiliations
    intersection.push_back(line.diffline);
  }
  return intersection;
}

DiffResult intersection(std::vector<DiffResult> diffResults)
{
  std::vector<std::vector<DiffLine>> addedLinesPerDiff;
  for (const auto& result : diffResults)
  {
    addedLinesPerDiff.push_back(result.added_lines);
  }

  std::vector<std::vector<DiffLine>> removedLinesPerDiff;
  for (const auto& result : diffResults)
  {
    removedLinesPerDiff.push_back(result.removed_lines);
  }

  std::vector<std::set<size_t>> addedFeaturesPerDiff;
  for (const auto& result : diffResults)
  {
    addedFeaturesPerDiff.push_back(result.info.added_features);
  }
  auto featureResultAdded { featureIntersection(addedFeaturesPerDiff) };

  std::vector<std::set<size_t>> RemovedFeaturesPerDiff;
  for (const auto& result : diffResults)
  {
    addedFeaturesPerDiff.push_back(result.info.added_features);
  }
  auto featureResultRemoved { featureIntersection(addedFeaturesPerDiff) };

  DiffResult intersectionResult;
  if (allSameSource(diffResults))
  {
    intersectionResult.added_lines
        = differentSourceIntersection(addedLinesPerDiff, featureResultAdded);
    intersectionResult.removed_lines = differentSourceIntersection(
        removedLinesPerDiff, featureResultRemoved);
    intersectionResult.info.added_features = featureResultAdded;
  }
  else
  {
    intersectionResult.added_lines
        = differentSourceIntersection(addedLinesPerDiff, featureResultAdded);
    intersectionResult.removed_lines = differentSourceIntersection(
        removedLinesPerDiff, featureResultRemoved);
    intersectionResult.info.removed_features = featureResultRemoved;
  }
  return intersectionResult;
}

std::vector<DiffLine> differentSourceSubtraction(
    std::vector<std::vector<DiffLine>> diffLinesPerFile,
    std::set<size_t>                   resultingFeatureAffiliations)
{
  std::vector<DiffLine> subtraction;
  if (diffLinesPerFile.empty())
  {
    return subtraction; // Return empty if no results
  }

  std::vector<std::vector<DiffLineCompareWOSource>> difflinesCompareWrapper {};
  for (const auto& diffLines : diffLinesPerFile)
  {
    std::vector<DiffLineCompareWOSource> difflines {};
    for (const auto& line : diffLines)
    {
      DiffLineCompareWOSource compareLine { line };
      difflines.push_back(compareLine);
    }
    difflinesCompareWrapper.push_back(difflines);
  }

  std::set<size_t> removedIndices {};
  for (size_t i { 1 }; i < difflinesCompareWrapper.size(); ++i)
  {
    auto res { difflinesCompareWrapper[i] };
    auto lcsResult { lcs(difflinesCompareWrapper[0], res) };
    for (auto index : lcsResult.leftIndices)
    {
      removedIndices.insert(index);
    }
  }
  for (size_t i {}; i < diffLinesPerFile[0].size(); ++i)
  {
    if (removedIndices.find(i) == removedIndices.end())
    {
      // If the index is not in the removed indices, add it to the subtraction
      auto diffLine { diffLinesPerFile[0][i] };
      diffLine.feature_affiliations
          = resultingFeatureAffiliations; // Set the feature affiliations
      subtraction.push_back(diffLine);
    }
    else
    {
      subtraction.push_back(diffLinesPerFile[0][i]);
    }
  }
  return subtraction;
}

std::vector<DiffLine>
    sameSourceSubtraction(std::vector<std::vector<DiffLine>> diffLinesPerFile,
                          std::set<size_t> resultingFeatureAffiliations)
{
  std::vector<DiffLine> subtraction;
  if (diffLinesPerFile.empty())
  {
    return subtraction; // Return empty if no results
  }
  std::set<size_t> removedIndices {};
  for (size_t i { 1 }; i < diffLinesPerFile.size(); ++i)
  {
    auto res { diffLinesPerFile[i] };
    auto lcsResult { lcs(diffLinesPerFile[0], res) };
    for (auto index : lcsResult.leftIndices)
    {
      removedIndices.insert(index);
    }
  }
  for (size_t i {}; i < diffLinesPerFile[0].size(); ++i)
  {
    if (removedIndices.find(i) == removedIndices.end())
    {
      // If the index is not in the removed indices, add it to the subtraction
      auto diffLine { diffLinesPerFile[0][i] };
      diffLine.feature_affiliations
          = resultingFeatureAffiliations; // Set the feature affiliations
      subtraction.push_back(diffLine);
    }
    else
    {
      subtraction.push_back(diffLinesPerFile[0][i]);
    }
  }
  return subtraction;
}

std::set<size_t> featureDifference(std::vector<std::set<size_t>> featureSets)
{
  if (featureSets.empty())
  {
    return {}; // Return empty if no feature sets
  }
  if (featureSets.size() == 1)
  {
    return featureSets[0];
  }

  std::set<size_t> difference = featureSets[0];
  for (size_t i = 1; i < featureSets.size(); ++i)
  {
    std::set<size_t> currentSet = featureSets[i];
    std::set<size_t> newDifference;
    set_difference(difference.begin(),
                   difference.end(),
                   currentSet.begin(),
                   currentSet.end(),
                   std::inserter(newDifference, newDifference.begin()));
    difference = newDifference;
  }

  return difference;
}

DiffResult subtraction(std::vector<DiffResult> diffResults)
{
  std::vector<std::vector<DiffLine>> addedLinesPerDiff;
  for (const auto& result : diffResults)
  {
    addedLinesPerDiff.push_back(result.added_lines);
  }

  std::vector<std::vector<DiffLine>> removedLinesPerDiff;
  for (const auto& result : diffResults)
  {
    removedLinesPerDiff.push_back(result.removed_lines);
  }

  std::vector<std::set<size_t>> addedFeaturesPerDiff;
  for (const auto& result : diffResults)
  {
    addedFeaturesPerDiff.push_back(result.info.added_features);
  }
  auto featureResultAdded { featureDifference(addedFeaturesPerDiff) };

  std::vector<std::set<size_t>> RemovedFeaturesPerDiff;
  for (const auto& result : diffResults)
  {
    addedFeaturesPerDiff.push_back(result.info.added_features);
  }
  auto featureResultRemoved { featureDifference(addedFeaturesPerDiff) };

  DiffResult subtractionResult;
  subtractionResult.added_lines
      = differentSourceSubtraction(addedLinesPerDiff, featureResultAdded);
  subtractionResult.removed_lines
      = differentSourceSubtraction(removedLinesPerDiff, featureResultRemoved);
  subtractionResult.info.added_features   = featureResultAdded;
  subtractionResult.info.removed_features = featureResultRemoved;
  return subtractionResult;
}

std::vector<std::pair<Node*, std::string>>
    addColorsToNodes(std::vector<Node*> nodes)
{
  std::vector<std::pair<Node*, std::string>> nodesWithColors {};
  std::stack<std::string>                    colorStack {
                       { "#FFFACD",
                        "#D0F0C0", "#FFECB3",
                        "#CCE5FF", "#FFDDE1",
                        "#E0BBE4", "#F0E68C",
                        "#D5F4E6", "#FAD6A5",
                        "#E6E6FA" }
  };
  std::map<std::set<size_t>, std::string> featureColorMap {};
  for (auto& node : nodes)
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

void analyzeFeature(const std::vector<DiffResult>& diffResults,
                    Configuration                  config,
                    size_t                         feature)
{
  auto filtered { filterDiffResultsByAddedFeatures(diffResults, { feature }) };
  std::sort(
      filtered.begin(),
      filtered.end(),
      [](const DiffResult& a, const DiffResult& b)
      { return a.info.added_features.size() <= b.info.added_features.size(); });
  auto differenceResult { intersection(filtered) };

  if (differenceResult.info.added_features.size() > 1)
  {
    std::vector<DiffResult> filteredSubtract { diffResults };
    for (auto& f : differenceResult.info.added_features)
    {
      std::cout << "Hello" << std::endl;
      if (f == feature)
      {
        continue;
      }

      filteredSubtract
          = filterDiffResultsByAddedFeatures(filteredSubtract, { f });
    }
    auto intersectionSubtract { intersection(filteredSubtract) };
    differenceResult = subtraction({ differenceResult, intersectionSubtract });
    if (differenceResult.info.added_features.size() > 1)
    {
      std::cout << "Cannot compute feature " << feature << ". Got features "
                << featuresToString(differenceResult.info.added_features)
                << std::endl;
    }
  }

  std::vector<std::pair<size_t, std::set<size_t>>> tokens;
  for (auto& line : differenceResult.added_lines)
  {
    tokens.push_back({ line.content, line.feature_affiliations });
  }
  auto file { filtered[0].info.dest };
  auto parsedFile { parseFile(file, "cpp") };
  auto parsedFileRaw { parsedFile.get() };
  auto matching { matchLCSWithTree<size_t>(tokens, parsedFileRaw) };
  auto nodesWithColors { addColorsToNodes(matching) };
  auto rendered { renderFile(file, config, nodesWithColors) };

  std::ofstream outFile("feature" + std::to_string(feature) + ".html");
  outFile << rendered;
  outFile.close();
}

void renderTokensToFile(std::vector<size_t> tokenHashes,
                        std::string         file,
                        std::string         featureName,
                        Configuration       config)
{
  std::vector<std::pair<size_t, std::set<size_t>>> tokens;
  for (auto& hash : tokenHashes)
  {
    tokens.push_back({ hash, { 1 } });
  }
  auto parsedFile { parseFile(file, "cpp") };
  auto parsedFileRaw { parsedFile.get() };
  auto matching { matchLCSWithTree<size_t>(tokens, parsedFileRaw) };
  auto nodesWithColors { addColorsToNodes(matching) };
  auto rendered { renderFile(file, config, nodesWithColors) };

  std::ofstream outFile("feature" + featureName + ".html");
  outFile << rendered;
  outFile.close();
}

void test(std::map<std::string, std::unique_ptr<Node>>& parsedFiles,
          const Configuration&                          config)
{
  std::vector<size_t> s1 {};
  std::vector<size_t> s2 {};
  std::vector<size_t> s3 {};
  std::vector<size_t> s4 {};
  for (auto& lv : parsedFiles["S1"]->getLeafs())
  {
    s1.push_back(lv->getSubtreeHash());
  }
  for (auto& lv : parsedFiles["S2"]->getLeafs())
  {
    s2.push_back(lv->getSubtreeHash());
  }
  for (auto& lv : parsedFiles["S3"]->getLeafs())
  {
    s3.push_back(lv->getSubtreeHash());
  }
  for (auto& lv : parsedFiles["S4"]->getLeafs())
  {
    s4.push_back(lv->getSubtreeHash());
  }
  auto res1_6 { runDiffs2(s1, s2) };
  auto res2_6 { runDiffs2(s1, s3) };
  auto res1_2_5_6 { runDiffs2(s1, s4) };
  auto res1 { runDiffs2(res2_6, res1_6) };
  auto res6 { runDiffs2(res1, res1_6) };
  auto res2 { runDiffs2(res6, res2_6) };
  auto res1_5 { runDiffs2(res2_6, res1_2_5_6) };
  auto res5 { runDiffs2(res1, res1_5) };

  renderTokensToFile(res1, "example/rl.hpp", "1", config);
  renderTokensToFile(res2, "example/rc.hpp", "2", config);
  renderTokensToFile(res6, "example/rl.hpp", "6", config);
  renderTokensToFile(res5, "example/rcl.hpp", "5", config);
  renderTokensToFile(res1_5, "example/rcl.hpp", "1_5", config);
}

void test2(std::map<std::string, std::unique_ptr<Node>>& parsedFiles,
           const Configuration&                          config)
{
  auto res { diffTrees(parsedFiles["S1"].get(), parsedFiles["S2"].get()) };

  for (auto& edit : res)
  {
    auto lineStart { (edit.type == Edit::Insert ? "+ " : "- ") };
    // auto leaves { edit.node->getLeafs() };
    // for (auto& leaf : leaves)
    // {
    //   std::cout << lineStart << leaf->getTsText() << std::endl;
    // }
    if (edit.node->isLeaf())
    {
      std::cout << lineStart << edit.node->getTsText() << std::endl;
    }
  }
}

void analyzeFeatures(const std::vector<DiffResult>& diffResults,
                     Configuration                  config)
{
  // Start with or-features
  // const auto& or_features { config.featureInfo.or_ };
  // for (size_t i { or_features.size() }; i >= 1; i--)
  // {
  //   auto current_i { i - 1 };
  //   auto current_feature { or_features[current_i] };
  // }

  // test(diffResults, config);
  // test2(diffResults, config);

  // analyzeFeature(diffResults, config, 1);
  // analyzeFeature(diffResults, config, 2);
  // analyzeFeature(diffResults, config, 3);
  // analyzeFeature(diffResults, config, 4);
  // analyzeFeature(diffResults, config, 5);
  // analyzeFeature(diffResults, config, 6);
  // analyzeFeature(diffResults, config, 7);
  // analyzeFeature(diffResults, config, 8);
}

void analyzeFeaturesDiff() { }

std::map<std::string, std::unique_ptr<Node>>
    parseFiles(const NamePathMappings& npms)
{
  std::map<std::string, std::unique_ptr<Node>> nameASTMap;
  for (auto& mapping : npms)
  {
    nameASTMap[mapping.first] = parseFile(mapping.second.paths[0], "cpp");
  }
  return nameASTMap;
}

std::vector<DiffInfo>
    buildDiffInfos(const NamePathMappings&                       npms,
                   std::map<std::string, std::unique_ptr<Node>>& nameASTMap)
{
  std::vector<DiffInfo> diffs;
  for (auto& mapping : npms)
  {
    for (auto& mapping2 : npms)
    {
      if (mapping.first == mapping2.first)
      {
        continue; // Skip comparing the same system
      }

      auto addedFeatures { featureDifference(
          { mapping2.second.containedFeatures,
            mapping.second.containedFeatures }) };

      auto removedFeatures { featureDifference(
          { mapping.second.containedFeatures,
            mapping2.second.containedFeatures }) };

      DiffInfo diffInfo { mapping.second.paths[0],
                          mapping2.second.paths[0],
                          nameASTMap[mapping.first].get(),
                          nameASTMap[mapping2.first].get(),
                          addedFeatures,
                          removedFeatures };
      diffs.push_back(diffInfo);
    }
  }
  return diffs;
}

void renderDiffInfos(std::vector<DiffInfo> diffs)
{
  for (auto& diff : diffs)
  {
    std::cout << "Src: " << diff.source << " Dest: " << diff.dest
              << " Added features: " << featuresToString(diff.added_features)
              << " Removed features: "
              << featuresToString(diff.removed_features) << std::endl;
  }
}

void featureLocation(Configuration config)
{
  // 1. Get all the diffs to run as DiffInfo
  auto namePathMappings = config.getNamePathMappings();
  auto parsedFiles { parseFiles(namePathMappings) };

  auto diffs { buildDiffInfos(namePathMappings, parsedFiles) };
  renderDiffInfos(diffs);

  test2(parsedFiles, config);

  // // 2. Run the diffinfos
  // auto diffResults { runDiffs(diffs) };
  //
  // // 3. Analyze the features starting with or
  // analyzeFeatures(diffResults, config);
}
