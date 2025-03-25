#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

struct BasePlusRelativePath
{
    std::filesystem::path base {};
    std::filesystem::path relative {};

    std::filesystem::path fullPath() const
    {
      return relative == "" ? base : base / relative;
    }
};

template<typename T> class Expression
{
  public:
    std::vector<T>           leftSide;
    std::vector<T>           rightSide;
    std::vector<std::string> labels;

    void render() const
    {
      std::cout << "  Expression: " << std::endl;
      std::cout << "    Left Side: " << std::endl;
      for (const auto &node : this->leftSide)
      {
        std::cout << "      " << node << std::endl;
      }
      std::cout << "    Right Side: " << std::endl;
      for (const auto &node : this->rightSide)
      {
        std::cout << "      " << node << std::endl;
      }
      std::cout << "    Labels: " << std::endl;
      for (const auto &label : this->labels)
      {
        std::cout << "      " << label << std::endl;
      }
    }
};

template<typename U> class Expression<std::vector<U>>
{
  public:
    std::vector<std::vector<U>> leftSide;
    std::vector<std::vector<U>> rightSide;
    std::vector<std::string>    labels;

    void render() const
    {
      std::cout << "  Expression: " << std::endl;
      std::cout << "    Left Side: " << std::endl;
      for (const auto &node : this->leftSide)
      {
        std::cout << "      [";
        for (const auto &file : node)
        {
          std::cout << file << ", ";
        }
        std::cout << "]" << std::endl;
      }
      std::cout << "    Right Side: " << std::endl;
      for (const auto &node : this->rightSide)
      {
        std::cout << "      [";
        for (const auto &file : node)
        {
          std::cout << file << ", ";
        }
        std::cout << "]" << std::endl;
      }
      std::cout << "    Labels: " << std::endl;
      for (const auto &label : this->labels)
      {
        std::cout << "      " << label << std::endl;
      }
    }
};

using ExpressionSystemName = Expression<std::string>; // S1, S2, S3, ...
using ExpressionAllFiles
    = Expression<std::vector<BasePlusRelativePath>>; // contains only files
using SingleFileExpression
    = Expression<BasePlusRelativePath>; // Each element is a single file

#endif // EXPRESSION_HPP
