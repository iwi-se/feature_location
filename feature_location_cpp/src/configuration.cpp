#include "configuration.hpp"
#include <iostream>
#include <yaml-cpp/yaml.h>

Configuration::Configuration(const std::string &filename)
{
  YAML::Node config = YAML::LoadFile(filename);

  action = config["action"].as<std::string>();

  for (const auto &mapping : config["name-path-mappings"])
  {
    namePathMappings[mapping["name"].as<std::string>()] = {};
    for (const auto &path : mapping["paths"])
    {
      namePathMappings[mapping["name"].as<std::string>()].push_back(
          path.as<std::string>());
    }
  }

  options.minimumTraceWeight
      = config["options"]["minimum_trace_weight"].as<int>();
  options.onlyNamedNodes = config["options"]["only_named_nodes"].as<bool>();
  options.language         = config["options"]["language"].as<std::string>();
  options.debug            = config["options"]["debug"].as<bool>();

  if (config["options"]["only_specific_nodes"])
  {
    Options::OnlySpecificNodes onlySpecificNodes;
    onlySpecificNodes.nodeTypesFile
        = config["options"]["only_specific_nodes"]["node_types_file"]
              .as<std::string>();
    onlySpecificNodes.nodeTypes
        = config["options"]["only_specific_nodes"]["node_types"]
              .as<std::vector<std::string>>();
    options.onlySpecificNodes
        = std::optional<Options::OnlySpecificNodes>(onlySpecificNodes);
  }

  for (const auto &expression : config["expressions"])
  {
    ExpressionSystemName expr;
    expr.leftSide  = expression["left-side"].as<std::vector<std::string>>();
    expr.rightSide = expression["right-side"].as<std::vector<std::string>>();
    expr.labels     = expression["labels"].as<std::vector<std::string>>();
    expressions.push_back(expr);
  }

  run       = config["run"].as<std::vector<std::string>>();
  basePath = std::filesystem::path(filename).parent_path();
}

void Configuration::render()
{
  std::cout << "Base Path: " << basePath << std::endl;
  std::cout << "Action: " << action << std::endl;
  std::cout << "Name Path Mappings: " << std::endl;
  for (const auto &mapping : namePathMappings)
  {
    std::cout << "  " << mapping.first << ": " << std::endl;
    for (const auto &path : mapping.second)
    {
      std::cout << "    " << path << std::endl;
    }
  }
  std::cout << "Options: " << std::endl;
  std::cout << "  Minimum Trace Weight: " << options.minimumTraceWeight
            << std::endl;
  std::cout << "  Only Named Nodes: " << options.onlyNamedNodes << std::endl;
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

std::vector<ExpressionSystemName>
    Configuration::getExpressionsToEvaluate() const
{
  std::vector<ExpressionSystemName> expressionsToEvaluate;
  for (const auto &expression : this->expressions)
  {
    for (const auto &label : this->run)
    {
      if (std::find(expression.labels.begin(), expression.labels.end(), label)
          != expression.labels.end())
      {
        expressionsToEvaluate.push_back(expression);
      }
    }
  }
  return expressionsToEvaluate;
}

std::vector<std::string>
    Configuration::getPathsForSystem(const std::string &systemName) const
{
  return namePathMappings.at(systemName);
}

bool Configuration::fileExtensionMatchesLanguage(
    const std::filesystem::path &path) const
{
  if (options.language == "cpp")
  {
    return std::find(cppFileExtensions.begin(),
                     cppFileExtensions.end(),
                     path.extension().string())
           != cppFileExtensions.end();
  }
  else if (options.language == "java")
  {
    return std::find(javaFileExtensions.begin(),
                     javaFileExtensions.end(),
                     path.extension().string())
           != javaFileExtensions.end();
  }
  return false;
}
