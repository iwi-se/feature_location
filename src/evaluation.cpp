#include <vector>
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

SingleFileExpressionResult evaluateExpression(SingleFileExpression expression,
                                              Configuration &config)
{
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
  return result;
}

class EvaluateExpression
{
  public:
    EvaluateExpression(SingleFileExpression expression,
                       Configuration &config)
        : expression(expression)
        , config(config)
    { }

    void run()
    {
      differenceResult = evaluateExpression(expression, config);
    }

    SingleFileExpression       expression;
    Configuration       &config;
    SingleFileExpressionResult differenceResult;
};

std::vector<SingleFileExpressionResult>
    runExpression(ExpressionSystemName expression, Configuration &config)
{
  ExpressionAllFiles expressionFiles = getExpressionFiles(expression, config);
  std::vector<SingleFileExpression> subexpressions
      = buildFileBasedSubExpressions(expressionFiles);
  std::vector<SingleFileExpressionResult> singleFileResults {};

  // Print the expression being processed
  std::cout << "Processing expression: " << expression.labels[0] << " ("
            << subexpressions.size() << " files) " << std::endl;

  // Progress tracking variables
  int totalFiles = subexpressions.size();

  std::vector<std::thread>          threads {};
  std::vector<EvaluateExpression *> evaluateExpressions {};
  for (size_t i = 0; i < subexpressions.size(); ++i)
  {
    auto evaluateExpression
        = new EvaluateExpression(subexpressions[i], config);
    threads.push_back(
        std::thread(&EvaluateExpression::run, evaluateExpression));
    evaluateExpressions.push_back(evaluateExpression);
  }

  for (size_t i = 0; i < threads.size(); ++i)
  {
    std::cout << "Waiting for " << expression.labels[0] << " ("
              << subexpressions.size() << " files) " << (i + 1) << "/"
              << totalFiles << " " << subexpressions[i].leftSide[0].relative
              << std::endl;
    threads[i].join();
    std::cout << "Joined " << (i + 1) << "/" << totalFiles << std::endl;
    auto &expressionResult { evaluateExpressions[i]->differenceResult };
    expressionResult.differenceResult.relativePath
        = subexpressions[i].leftSide[0].relative;
    singleFileResults.push_back(std::move(evaluateExpressions[i]->differenceResult));
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
    std::vector<BasePlusRelativePath> firstLeftSideSystem
        = expression.leftSide[0];
    for (const auto &file : firstLeftSideSystem)
    {
      SingleFileExpression singleFileExpression {};
      singleFileExpression.leftSide.push_back(file);
      for (size_t i = 1; i < expression.leftSide.size(); i++)
      {
        const std::vector<BasePlusRelativePath> otherLeftSideSystem {
          expression.leftSide[i]
        };
        for (const auto &otherFile : otherLeftSideSystem)
        {
          if (file.relative == otherFile.relative)
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
          if (file.relative == rightSideFile.relative)
          {
            singleFileExpression.rightSide.push_back(rightSideFile);
            singleFileExpression.labels = expression.labels;
            break;
          }
        }
      }

      if (singleFileExpression.leftSide.size() == expression.leftSide.size())
      {
        subexpressions.push_back(singleFileExpression);
      }
    }
  }
  return subexpressions;
}
