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
#include <vector>

SingleFileExpressionResult evaluateExpression(SingleFileExpression expression,
                                              Configuration       &config)
{
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<std::unique_ptr<Node>> leftSideTrees {};
  for (const auto &leftSideSystem : expression.leftSide)
  {
    std::filesystem::path fullPath
        = config.basePath / leftSideSystem.fullPath();
    leftSideTrees.push_back(parseFile(fullPath, config.options.language));
    if (config.options.debug)
    {
      std::cout << "Parsed file: " << fullPath << std::endl;
      leftSideTrees.back()->render(0);
    }
  }
  std::vector<std::unique_ptr<Node>> rightSideTrees {};
  for (const auto &rightSideSystem : expression.rightSide)
  {
    std::filesystem::path fullPath
        = config.basePath / rightSideSystem.fullPath();
    rightSideTrees.push_back(parseFile(fullPath, config.options.language));
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
    while (fp == "/dev/null");

    size_t size { std::filesystem::file_size(fp) };
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

  size_t                                         numberOfThreads { 7 };
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

std::vector<BasePlusRelativePath> getFilesForSystem(std::string systemName,
                                                    const Configuration &config)
{
  std::vector<std::string> paths = config.getPathsForSystem(systemName);
  std::vector<BasePlusRelativePath> allFiles {};
  for (const auto &path : paths)
  {
    std::filesystem::path p(path);
    if (p.has_extension() && config.fileExtensionMatchesLanguage(p))
    {
      allFiles.push_back(BasePlusRelativePath { p });
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
          allFiles.push_back(BasePlusRelativePath { p, relativePath });
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
    std::vector<BasePlusRelativePath> files
        = getFilesForSystem(systemName, config);
    expressionAllFiles.leftSide.push_back(files);
  }
  for (const auto &systemName : expression.rightSide)
  {
    std::vector<BasePlusRelativePath> files
        = getFilesForSystem(systemName, config);
    expressionAllFiles.rightSide.push_back(files);
  }
  expressionAllFiles.labels = expression.labels;
  return expressionAllFiles;
}

std::vector<BasePlusRelativePath> getAllFiles(ExpressionAllFiles expression)
{
  std::vector<std::vector<BasePlusRelativePath>> allSystems {};
  allSystems.insert(
      allSystems.end(), expression.leftSide.begin(), expression.leftSide.end());
  allSystems.insert(allSystems.end(),
                    expression.rightSide.begin(),
                    expression.rightSide.end());
  std::vector<BasePlusRelativePath> allFiles {};
  for (const auto &system : allSystems)
  {
    allFiles.insert(allFiles.end(), system.begin(), system.end());
  }
  std::sort(allFiles.begin(), allFiles.end());
  allFiles.erase(std::unique(allFiles.begin(), allFiles.end()), allFiles.end());
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
      singleFileExpression.leftSide.push_back(leftSideSystem[0]);
    }
    for (const auto &rightSideSystem : expression.rightSide)
    {
      singleFileExpression.rightSide.push_back(rightSideSystem[0]);
    }
    singleFileExpression.labels = expression.labels;
    subexpressions.push_back(singleFileExpression);
  }
  else
  {
    std::vector<BasePlusRelativePath> allFiles { getAllFiles(expression) };
    for (const auto &fileName : allFiles)
    {
      SingleFileExpression singleFileExpression {};
      for (size_t i = 0; i < expression.leftSide.size(); i++)
      {
        const std::vector<BasePlusRelativePath> otherLeftSideSystem {
          expression.leftSide[i]
        };
        for (const auto &otherFile : otherLeftSideSystem)
        {
          if (fileName.relative == otherFile.relative)
          {
            singleFileExpression.leftSide.push_back(otherFile);
            singleFileExpression.labels = expression.labels;
            break;
          }
        }
      }
      for (const auto &rightSideSystem : expression.rightSide)
      {
        for (const auto &rightSideFile : rightSideSystem)
        {
          if (fileName.relative == rightSideFile.relative)
          {
            singleFileExpression.rightSide.push_back(rightSideFile);
            singleFileExpression.labels = expression.labels;
            break;
          }
        }
      }

      while (singleFileExpression.leftSide.size() < expression.leftSide.size())
      {
        singleFileExpression.leftSide.insert(
            singleFileExpression.leftSide.begin(),
            BasePlusRelativePath { "/dev/null", "" });
      }
      subexpressions.push_back(singleFileExpression);
    }
  }
  return subexpressions;
}
