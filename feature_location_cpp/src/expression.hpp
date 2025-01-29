#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

template <typename T>
class Expression
{
public:
    std::vector<T> left_side;
    std::vector<T> right_side;
    std::vector<std::string> labels;
    void render() const
    {
        std::cout << "  Expression: " << std::endl;
        std::cout << "    Left Side: " << std::endl;
        for (const auto &node : this->left_side)
        {
            std::cout << "      " << node << std::endl;
        }
        std::cout << "    Right Side: " << std::endl;
        for (const auto &node : this->right_side)
        {
            std::cout << "      " << node << std::endl;
        }
        std::cout << "    Labels: " << std::endl;
        for (const auto &label : this->labels)
        {
            std::cout << "      " << label << std::endl;
        }
    };
};

template <typename U>
class Expression<std::vector<U>>
{
public:
    std::vector<std::vector<U>> left_side;
    std::vector<std::vector<U>> right_side;
    std::vector<std::string> labels;
    void render() const
    {
        std::cout << "  Expression: " << std::endl;
        std::cout << "    Left Side: " << std::endl;
        for (const auto &node : this->left_side)
        {
            std::cout << "      [";
            for (const auto &file : node)
            {
                std::cout << file << ", ";
            }
            std::cout << "]" << std::endl;
        }
        std::cout << "    Right Side: " << std::endl;
        for (const auto &node : this->right_side)
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

using ExpressionSystemName = Expression<std::string>;                 // S1, S2, S3, ...
using ExpressionAllFiles = Expression<std::vector<std::filesystem::path>>;      // contains only files
using SingleFileExpression = Expression<std::filesystem::path>;                 // Each element is a single file

#endif // EXPRESSION_HPP