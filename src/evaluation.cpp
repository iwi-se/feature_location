#include "evaluation.hpp"
#include "parser.hpp"
#include "render.hpp"
#include "set_operations.hpp"
#include "tree.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <iostream>
#include <iomanip>

DifferenceResult evaluateExpression(SingleFileExpression expression,
                                    Configuration        config)
{
  std::vector<std::shared_ptr<Node>> leftSideTrees {};
  for (const auto &leftSideSystem : expression.leftSide)
  {
    std::filesystem::path fullPath
        = config.basePath / leftSideSystem.fullPath();
    std::shared_ptr<Node> root = parseFile(fullPath, config.options.language);
    if (config.options.debug)
    {
      std::cout << "Parsed file: " << fullPath << std::endl;
      root->render(0);
    }
    leftSideTrees.push_back(root);
  }
  std::vector<std::shared_ptr<Node>> rightSideTrees {};
  for (const auto &rightSideSystem : expression.rightSide)
  {
    std::filesystem::path fullPath
        = config.basePath / rightSideSystem.fullPath();
    std::shared_ptr<Node> root = parseFile(fullPath, config.options.language);
    rightSideTrees.push_back(root);
  }
  auto differenceResult { difference(leftSideTrees, rightSideTrees, config) };
  differenceResult.relativePath = expression.leftSide[0].relative;
  return differenceResult;
}

std::vector<DifferenceResult> runExpression(ExpressionSystemName expression,
                                            Configuration        config)
{
  ExpressionAllFiles expressionFiles = getExpressionFiles(expression, config);
  std::vector<SingleFileExpression> subexpressions
      = buildFileBasedSubExpressions(expressionFiles);
  std::vector<DifferenceResult> differenceResults {};
  
  // Print the expression being processed
  std::cout << "Processing expression: " << expression.labels[0] << " (" 
            << subexpressions.size() << " files) ";
  std::cout.flush();
  
  // Progress tracking variables
  int totalFiles = subexpressions.size();
  
  for (size_t i = 0; i < subexpressions.size(); ++i)
  {
    // Update file counter for every file
    std::cout << "\rProcessing expression: " << expression.labels[0] << " (" 
              << subexpressions.size() << " files) " << (i+1) << "/" << totalFiles;
    std::cout.flush();

    auto differenceResult { evaluateExpression(subexpressions[i], config) };
    differenceResult.relativePath = subexpressions[i].leftSide[0].relative;
    differenceResults.push_back(differenceResult);
  }
  
  // Print completed message
  std::cout << " done" << std::endl;
  
  return differenceResults;
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

std::vector<BasePlusRelativePath> getFilesForSystem(std::string   systemName,
                                                    Configuration config)
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
                                      Configuration        config)
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
