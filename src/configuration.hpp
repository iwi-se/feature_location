#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include "expression.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

using json = nlohmann::json;

struct System
{
    std::string              name;
    std::vector<std::string> paths;
    std::set<size_t>         containedFeatures;
};

using NamePathMappings = std::map<std::string, System>;

class Options

{
  public:
    struct OnlySpecificNodes
    {
        std::filesystem::path    nodeTypesFile;
        std::vector<std::string> nodeTypes;
    };

    int                              minimumTraceWeight;
    bool                             onlyNamedNodes;
    std::string                      language;
    bool                             debug {};
    std::optional<OnlySpecificNodes> onlySpecificNodes;
    json                             nodeTypes;
    std::vector<std::string>
        dynamicIncludedTypes; // memorizes at runtime which types are included
                              // to avoid recomputation
    std::vector<std::string>
        dynamicExcludedTypes; // memorizes at runtime which types are excluded
                              // to avoid recomputation
};

const std::vector<std::string> cppFileExtensions  = { ".cpp", ".h", ".hpp" };
const std::vector<std::string> javaFileExtensions = { ".java" };

class Configuration
{
  public:
    Configuration(const std::string &filename);
    void                              render();
    std::string                       getAction();
    std::vector<ExpressionSystemName> getExpressionsToEvaluate() const;
    std::vector<std::string>
        getPathsForSystem(const std::string &systemName) const;
    std::filesystem::path basePath;
    Options               options;
    bool fileExtensionMatchesLanguage(const std::filesystem::path &path) const;
    NamePathMappings getNamePathMappings() const;
  private:
    std::string                       action;
    NamePathMappings                  namePathMappings;
    std::vector<ExpressionSystemName> expressions;
    std::vector<std::string>          run;
};

#endif // CONFIGURATION_HPP
