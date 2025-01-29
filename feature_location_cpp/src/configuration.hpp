#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <map>

#include "expression.hpp"

using NamePathMappings =
    std::map<std::string, std::vector<std::string>>;

class Options
{
public:
    int minimum_trace_size;
    bool only_named_nodes;
    std::string language;
};

class Configuration
{
public:
    Configuration(const std::string &filename);
    ~Configuration();
    void render();
    std::vector<ExpressionSystemName> getExpressionsToEvaluate();
    std::vector<std::string> getPathsForSystem(std::string systemName);
    std::filesystem::path base_path;

private:
    std::string action;
    NamePathMappings name_path_mappings;
    Options options;
    std::vector<ExpressionSystemName> expressions;
    std::vector<std::string> run;
};

#endif // CONFIGURATION_HPP