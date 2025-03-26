#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include "expression.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

using NamePathMappings = std::map<std::string, std::vector<std::string>>;

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
};

const std::vector<std::string> cppFileExtensions  = { ".cpp", ".h", ".hpp" };
const std::vector<std::string> javaFileExtensions = { ".java" };

class Configuration
{
  public:
    Configuration(const std::string &filename);
    void                              render();
    std::vector<ExpressionSystemName> getExpressionsToEvaluate() const;
    std::vector<std::string>
        getPathsForSystem(const std::string &systemName) const;
    std::filesystem::path basePath;
    Options               options;
    bool fileExtensionMatchesLanguage(const std::filesystem::path &path) const;
  private:
    std::string                       action;
    NamePathMappings                  namePathMappings;
    std::vector<ExpressionSystemName> expressions;
    std::vector<std::string>          run;
};

#endif // CONFIGURATION_HPP
