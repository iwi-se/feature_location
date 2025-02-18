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
    struct OnlySpecificNodes
    {
        std::filesystem::path node_types_file;
        std::vector<std::string> node_types;
    };

    int minimum_trace_weight;
    bool only_named_nodes;
    std::string language;
    bool debug {};
    OnlySpecificNodes only_specific_nodes;

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
    Options options;

private:
    std::string action;
    NamePathMappings name_path_mappings;
    std::vector<ExpressionSystemName> expressions;
    std::vector<std::string> run;
};

#endif // CONFIGURATION_HPP