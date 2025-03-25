#include "evaluation.hpp"
#include "parser.hpp"
#include "render.hpp"
#include "set_operations.hpp"
#include "tree.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

DifferenceResult evaluateExpression(SingleFileExpression expression,
                                    Configuration        config)
{
  std::vector<std::shared_ptr<Node>> left_side_trees {};
  for (const auto &left_side_system : expression.left_side)
  {
    std::filesystem::path full_path
        = config.base_path / left_side_system.fullPath();
    std::shared_ptr<Node> root = parse_file(full_path, config.options.language);
    if (config.options.debug)
    {
      std::cout << "Parsed file: " << full_path << std::endl;
      root->render(0);
    }
    left_side_trees.push_back(root);
  }
  std::vector<std::shared_ptr<Node>> right_side_trees {};
  for (const auto &right_side_system : expression.right_side)
  {
    std::filesystem::path full_path
        = config.base_path / right_side_system.fullPath();
    std::shared_ptr<Node> root = parse_file(full_path, config.options.language);
    right_side_trees.push_back(root);
  }
  auto difference_result { difference(
      left_side_trees, right_side_trees, config) };
  difference_result.relative_path = expression.left_side[0].relative;
  return difference_result;
}

std::vector<DifferenceResult> runExpression(ExpressionSystemName expression,
                                            Configuration        config)
{
  ExpressionAllFiles expressionFiles = getExpressionFiles(expression, config);
  std::vector<SingleFileExpression> subexpressions
      = buildFileBasedSubExpressions(expressionFiles);
  std::vector<DifferenceResult> difference_results {};
  for (const auto &subexpression : subexpressions)
  {
    auto difference_result { evaluateExpression(subexpression, config) };
    difference_result.relative_path = subexpression.left_side[0].relative;
    difference_results.push_back(difference_result);
  }
  return difference_results;
}

bool hasOnlySingleFileSystems(ExpressionAllFiles expression)
{
  bool length_is_one = true;
  for (const auto &left_side_system : expression.left_side)
  {
    if (left_side_system.size() != 1)
    {
      length_is_one = false;
    }
  }
  for (const auto &right_side_system : expression.right_side)
  {
    if (right_side_system.size() != 1)
    {
      length_is_one = false;
    }
  }
  return length_is_one;
}

std::vector<BasePlusRelativePath> getFilesForSystem(std::string   systemName,
                                                    Configuration config)
{
  std::vector<std::string> paths = config.getPathsForSystem(systemName);
  std::vector<BasePlusRelativePath> allFiles {};
  for (const auto &path : paths)
  {
    std::filesystem::path p(path);
    if (p.has_extension() && config.file_extension_matches_language(p))
    {
      allFiles.push_back(BasePlusRelativePath { p });
    }
    else
    {
      for (const auto &entry : std::filesystem::recursive_directory_iterator(p))
      {
        if (entry.is_regular_file()
            && config.file_extension_matches_language(entry.path()))
        {
          std::filesystem::path relative_path
              = std::filesystem::relative(entry.path(), p);
          allFiles.push_back(BasePlusRelativePath { p, relative_path });
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
  for (const auto &systemName : expression.left_side)
  {
    std::vector<BasePlusRelativePath> files
        = getFilesForSystem(systemName, config);
    expressionAllFiles.left_side.push_back(files);
  }
  for (const auto &systemName : expression.right_side)
  {
    std::vector<BasePlusRelativePath> files
        = getFilesForSystem(systemName, config);
    expressionAllFiles.right_side.push_back(files);
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
    for (const auto &left_side_system : expression.left_side)
    {
      singleFileExpression.left_side.push_back(left_side_system[0]);
    }
    for (const auto &right_side_system : expression.right_side)
    {
      singleFileExpression.right_side.push_back(right_side_system[0]);
    }
    singleFileExpression.labels = expression.labels;
    subexpressions.push_back(singleFileExpression);
  }
  else
  {
    std::vector<BasePlusRelativePath> first_left_side_system
        = expression.left_side[0];
    for (const auto &file : first_left_side_system)
    {
      SingleFileExpression singleFileExpression {};
      singleFileExpression.left_side.push_back(file);
      for (size_t i = 1; i < expression.left_side.size(); i++)
      {
        const std::vector<BasePlusRelativePath> other_left_side_system {
          expression.left_side[i]
        };
        for (const auto &other_file : other_left_side_system)
        {
          if (file.relative == other_file.relative)
          {
            singleFileExpression.left_side.push_back(other_file);
            singleFileExpression.labels = expression.labels;
            break;
          }
        }
      }
      for (const auto &right_side_system : expression.right_side)
      {
        for (const auto &right_side_file : right_side_system)
        {
          if (file.relative == right_side_file.relative)
          {
            singleFileExpression.right_side.push_back(right_side_file);
            singleFileExpression.labels = expression.labels;
            break;
          }
        }
      }

      if (singleFileExpression.left_side.size() == expression.left_side.size())
      {
        subexpressions.push_back(singleFileExpression);
      }
    }
  }
  std::cout << "Subexpressions: " << subexpressions.size() << std::endl;
  for (const auto &subexpression : subexpressions)
  {
    std::cout << "Subexpression: " << std::endl;
    for (const auto &left_side : subexpression.left_side)
    {
      std::cout << "Left Side: " << left_side.fullPath() << std::endl;
    }
    for (const auto &right_side : subexpression.right_side)
    {
      std::cout << "Right Side: " << right_side.fullPath() << std::endl;
    }
  }
  return subexpressions;
}
