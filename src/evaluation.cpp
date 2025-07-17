#include "evaluation.hpp"
#include "parser.hpp"
#include "render.hpp"
#include "set_operations.hpp"
#include "tree.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

std::vector<std::string> makeTokenTable(Node *n)
{
  std::vector<std::string> leafTexts;
  for (auto &leaf : n->getLeafs())
  {
    leafTexts.push_back(leaf->getTsText());
  }
  return leafTexts;
}

SingleFileExpressionResult evaluateExpression(SingleFileExpression expression,
                                              Configuration       &config)
{
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<std::unique_ptr<Node>> leftSideTrees {};
  for (const auto &leftSideSystem : expression.leftSide)
  {
    std::filesystem::path fullPath
        = config.basePath / leftSideSystem.fullPath();
    auto ast { parseFile(fullPath, config.options.language) };
    auto astTokens { makeTokenTable(ast.get()) };
    bool allDifferent { true };
    for (auto &tree : leftSideTrees)
    {
      auto newASTTokens { makeTokenTable(tree.get()) };
      if (astTokens == newASTTokens) // If files are equal, skip
      {
        allDifferent = false;
        break;
      }
    }
    if (allDifferent)
    {
      leftSideTrees.push_back(std::move(ast));
    }
    else
    {
      ast.reset(); // Free memory if not needed
    }
    if (config.options.debug)
    {
      std::cout << "Parsed file: " << fullPath << std::endl;
      leftSideTrees.back()->render(0);
    }
  }
  std::cout << leftSideTrees.size() << " leftside files of "
            << expression.leftSide.size() << " files are unique" << std::endl;
  std::vector<std::unique_ptr<Node>> rightSideTrees {};
  for (const auto &rightSideSystem : expression.rightSide)
  {
    std::filesystem::path fullPath
        = config.basePath / rightSideSystem.fullPath();
    auto ast { parseFile(fullPath, config.options.language) };
    auto astTokens { makeTokenTable(ast.get()) };
    bool allDifferent { true };
    for (auto &tree : rightSideTrees)
    {
      auto newASTTokens { makeTokenTable(tree.get()) };
      if (astTokens == newASTTokens) // If files are equal, skip
      {
        allDifferent = false;
        break;
      }
    }
    if (allDifferent)
    {
      rightSideTrees.push_back(std::move(ast));
    }
    else
    {
      ast.reset(); // Free memory if not needed
    }
    if (config.options.debug)
    {
      std::cout << "Parsed file: " << fullPath << std::endl;
      rightSideTrees.back()->render(0);
    }
  }
  auto differenceResult { difference(leftSideTrees, rightSideTrees, config) };
  SingleFileExpressionResult result { differenceResult,
                                      std::move(leftSideTrees),
                                      std::move(rightSideTrees) };
  auto endTime  = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration<double>(endTime - startTime);
  if (duration.count() > 2.0)
  {
    std::cout << "File " << expression.leftSide[0].fullPath()
              << " took a long time to process: " << std::fixed
              << std::setprecision(3) << duration.count() << " seconds"
              << std::endl;
  }
  return result;
}

class EvaluateExpressionThread
{
  public:
    EvaluateExpressionThread(std::vector<SingleFileExpression> expressions,
                             Configuration                    &config)
        : expressions(expressions)
        , config(config)
    { }

    void run()
    {
      for (const auto &expression : expressions)
      {
        differenceResults.push_back(evaluateExpression(expression, config));
      }
    }

    std::vector<SingleFileExpression>       expressions;
    Configuration                          &config;
    std::vector<SingleFileExpressionResult> differenceResults;
};

void sortSingleFileExpressions(std::vector<SingleFileExpression> &expressions)
{
  std::vector<std::pair<SingleFileExpression, size_t>> expressionsWithSize {};
  for (auto &expression : expressions)
  {
    size_t                i { 0 };
    std::filesystem::path fp;
    do
    {
      fp = expression.leftSide[i].fullPath();
      ++i;
    }
    while (fp == "/dev/null" && i < expression.leftSide.size());

    size_t size { fp == "/dev/null" ? 0 : std::filesystem::file_size(fp) };
    expressionsWithSize.push_back(std::make_pair(expression, size));
  }
  std::sort(expressionsWithSize.begin(),
            expressionsWithSize.end(),
            [](const std::pair<SingleFileExpression, size_t> &a,
               const std::pair<SingleFileExpression, size_t> &b)
            { return a.second > b.second; });
  expressions.clear();
  for (size_t i = 0; i < expressionsWithSize.size(); ++i)
  {
    expressions.push_back(expressionsWithSize[i].first);
  }
}

std::vector<SingleFileExpressionResult>
    runExpression(ExpressionSystemName expression, Configuration &config)
{
  ExpressionAllFiles expressionFiles = getExpressionFiles(expression, config);
  std::vector<SingleFileExpression> subexpressions
      = buildFileBasedSubExpressions(expressionFiles);
  sortSingleFileExpressions(subexpressions);
  std::vector<SingleFileExpressionResult> singleFileResults {};

  // Print the expression being processed
  std::cout << "Processing expression: " << expression.labels[0] << " ("
            << subexpressions.size() << " files) " << std::endl;

  // Progress tracking variables
  size_t totalFiles { subexpressions.size() };

  size_t                                         numberOfThreads { 3 };
  std::vector<std::thread>                       threads {};
  std::vector<EvaluateExpressionThread *>        evaluateExpressions {};
  std::vector<std::vector<SingleFileExpression>> expressionsForThread {
    numberOfThreads
  };
  size_t currentThread { 0 };
  for (size_t i = 0; i < subexpressions.size(); ++i)
  {
    expressionsForThread[currentThread].push_back(subexpressions[i]);
    if (currentThread == numberOfThreads - 1)
    {
      currentThread = 0;
    }
    else
    {
      ++currentThread;
    }
  }

  for (auto threadExpressions : expressionsForThread)
  {
    if (!threadExpressions.empty())
    {
      auto evaluateExpression
          = new EvaluateExpressionThread(threadExpressions, config);
      threads.push_back(
          std::thread(&EvaluateExpressionThread::run, evaluateExpression));
      evaluateExpressions.push_back(evaluateExpression);
    }
  }

  std::cout << "Running " << threads.size() << " threads" << std::endl;

  for (size_t i = 0; i < threads.size(); ++i)
  {
    std::cout << "Waiting for thread " << i << "/" << threads.size()
              << "( thread processes "
              << evaluateExpressions[i]->expressions.size() << " files)"
              << std::endl;
    threads[i].join();
    std::cout << "Joined " << i << "/" << threads.size() << std::endl;
    auto &expressionResults { evaluateExpressions[i]->differenceResults };
    for (size_t j = 0; j < expressionResults.size(); ++j)
    {
      expressionResults[j].differenceResult.relativePath
          = evaluateExpressions[i]->expressions[j].leftSide[0].relative;
      singleFileResults.push_back(std::move(expressionResults[j]));
    }
  }

  for (auto &evaluateExpression : evaluateExpressions)
  {
    delete evaluateExpression;
  }

  // Print completed message
  std::cout << "\nProcessing expressions done." << std::endl;

  return singleFileResults;
}

bool hasOnlySingleFileSystems(ExpressionAllFiles expression)
{
  bool lengthIsOne = true;
  for (const auto &leftSideSystem : expression.leftSide)
  {
    if (leftSideSystem.size() != 1)
    {
      lengthIsOne = false;
    }
  }
  for (const auto &rightSideSystem : expression.rightSide)
  {
    if (rightSideSystem.size() != 1)
    {
      lengthIsOne = false;
    }
  }
  return lengthIsOne;
}

std::unordered_set<BasePlusRelativePath>
    getFilesForSystem(std::string systemName, const Configuration &config)
{
  std::vector<std::string> paths = config.getPathsForSystem(systemName);
  std::unordered_set<BasePlusRelativePath> allFiles {};
  for (const auto &path : paths)
  {
    std::filesystem::path p(path);
    if (p.has_extension() && config.fileExtensionMatchesLanguage(p))
    {
      allFiles.insert(BasePlusRelativePath { p });
    }
    else
    {
      for (const auto &entry : std::filesystem::recursive_directory_iterator(p))
      {
        if (entry.is_regular_file()
            && config.fileExtensionMatchesLanguage(entry.path()))
        {
          std::filesystem::path relativePath
              = std::filesystem::relative(entry.path(), p);
          allFiles.insert(BasePlusRelativePath { p, relativePath });
        }
      }
    }
  }
  return allFiles;
}

ExpressionAllFiles getExpressionFiles(ExpressionSystemName expression,
                                      const Configuration &config)
{
  ExpressionAllFiles expressionAllFiles {};
  for (const auto &systemName : expression.leftSide)
  {
    auto files = getFilesForSystem(systemName, config);
    expressionAllFiles.leftSide.push_back(files);
  }
  for (const auto &systemName : expression.rightSide)
  {
    auto files = getFilesForSystem(systemName, config);
    expressionAllFiles.rightSide.push_back(files);
  }
  expressionAllFiles.labels = expression.labels;
  return expressionAllFiles;
}

std::unordered_set<BasePlusRelativePath>
    getAllFiles(ExpressionAllFiles expression)
{
  std::unordered_set<BasePlusRelativePath> allFiles {
    expression.leftSide[0].size() * 4
  };
  for (const auto &system : expression.leftSide)
  {
    for (const auto &file : system)
    {
      allFiles.insert(file);
    }
  }
  for (const auto &system : expression.rightSide)
  {
    for (const auto &file : system)
    {
      allFiles.insert(file);
    }
  }
  return allFiles;
}

std::vector<SingleFileExpression>
    buildFileBasedSubExpressions(ExpressionAllFiles expression)
{
  std::vector<SingleFileExpression> subexpressions {};
  if (hasOnlySingleFileSystems(expression))
  {
    SingleFileExpression singleFileExpression {};
    for (const auto &leftSideSystem : expression.leftSide)
    {
      singleFileExpression.leftSide.push_back(*leftSideSystem.begin());
    }
    for (const auto &rightSideSystem : expression.rightSide)
    {
      singleFileExpression.rightSide.push_back(*rightSideSystem.begin());
    }
    singleFileExpression.labels = expression.labels;
    subexpressions.push_back(singleFileExpression);
  }
  else
  {
    std::unordered_set<BasePlusRelativePath> allFiles { getAllFiles(
        expression) };
    std::cout << "start" << std::endl;
    for (const auto &fileName : allFiles)
    {
      SingleFileExpression singleFileExpression {};
      for (size_t i = 0; i < expression.leftSide.size(); i++)
      {
        const auto &leftSideSystem { expression.leftSide[i] };
        auto        it { leftSideSystem.find(fileName) };
        if (it != leftSideSystem.end())
        {
          singleFileExpression.leftSide.push_back(*it);
          singleFileExpression.labels = expression.labels;
          break;
        }
        else
        {
          singleFileExpression.leftSide.push_back(
              BasePlusRelativePath { "/dev/null", "" });
        }
      }
      for (const auto &rightSideSystem : expression.rightSide)
      {
        auto it { rightSideSystem.find(fileName) };
        if (it != rightSideSystem.end())
        {
          singleFileExpression.rightSide.push_back(*it);
          singleFileExpression.labels = expression.labels;
          break;
        }
      }

      subexpressions.push_back(singleFileExpression);
    }
  }

  std::cout << "stop" << std::endl;
  return subexpressions;
}
