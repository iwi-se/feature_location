#include <yaml-cpp/yaml.h>
#include "configuration.hpp"
#include <iostream>

Configuration::Configuration(const std::string &filename)
{
    YAML::Node config = YAML::LoadFile(filename);

    action = config["action"].as<std::string>();

    for (const auto &mapping : config["name-path-mappings"])
    {
        name_path_mappings[mapping["name"].as<std::string>()] = {};
        for (const auto &path : mapping["paths"])
        {
            name_path_mappings[mapping["name"].as<std::string>()].push_back(path.as<std::string>());
        }
    }

    options.minimum_trace_size = config["options"]["minimum_trace_size"].as<int>();
    options.only_named_nodes = config["options"]["only_named_nodes"].as<bool>();
    options.language = config["options"]["language"].as<std::string>();
    options.debug = config["options"]["debug"].as<bool>();

    for (const auto &expression : config["expressions"])
    {
        ExpressionSystemName expr;
        expr.left_side = expression["left-side"].as<std::vector<std::string>>();
        expr.right_side = expression["right-side"].as<std::vector<std::string>>();
        expr.labels = expression["labels"].as<std::vector<std::string>>();
        expressions.push_back(expr);
    }

    run = config["run"].as<std::vector<std::string>>();
    base_path = std::filesystem::path(filename).parent_path();
}

void Configuration::render()
{
    std::cout << "Base Path: " << base_path << std::endl;
    std::cout << "Action: " << action << std::endl;
    std::cout << "Name Path Mappings: " << std::endl;
    for (const auto &mapping : name_path_mappings)
    {
        std::cout << "  " << mapping.first << ": " << std::endl;
        for (const auto &path : mapping.second)
        {
            std::cout << "    " << path << std::endl;
        }
    }
    std::cout << "Options: " << std::endl;
    std::cout << "  Minimum Trace Size: " << options.minimum_trace_size << std::endl;
    std::cout << "  Only Named Nodes: " << options.only_named_nodes << std::endl;
    std::cout << "  Language: " << options.language << std::endl;
    std::cout << "Expressions: " << std::endl;
    for (const auto &expression : expressions)
    {
        expression.render();
    }
    std::cout << "Run: " << std::endl;
    for (const auto &path : run)
    {
        std::cout << "  " << path << std::endl;
    }
}

Configuration::~Configuration()
{
}

std::vector<ExpressionSystemName> Configuration::getExpressionsToEvaluate()
{
    std::vector<ExpressionSystemName> expressionsToEvaluate;
    for (const auto &expression : this->expressions)
    {
        for (const auto &label : this->run)
        {
            if (std::find(expression.labels.begin(), expression.labels.end(), label) != expression.labels.end())
            {
                expressionsToEvaluate.push_back(expression);
            }
        }
    }
    return expressionsToEvaluate;
}

std::vector<std::string> Configuration::getPathsForSystem(std::string systemName)
{
    return name_path_mappings[systemName];
}