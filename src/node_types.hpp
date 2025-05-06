#ifndef NODE_TYPES_HPP
#define NODE_TYPES_HPP

#include <fstream>
#include <nlohmann/json.hpp>
#include <stack>
#include <string>
#include <vector>

using json = nlohmann::json;

std::vector<std::string> getSupertypes(const json        &nodeTypes,
                                       const std::string &type)
{
  std::vector<std::string> supertypes;

  // Use stack for iterative traversal
  std::stack<std::string> stack;
  stack.push(type);

  while (!stack.empty())
  {
    std::string current = stack.top();
    stack.pop();

    for (const auto &nodeType : nodeTypes)
    {
      if (nodeType.contains("subtypes"))
      {
        for (const auto &subtype : nodeType["subtypes"])
        {
          if (subtype["type"] == current)
          {
            supertypes.push_back(nodeType["type"]);
            stack.push(nodeType["type"]);
          }
        }
      }
    }
  }

  return supertypes;
}

bool isIncludedNodeType(Node *node, Configuration &config)
{
  if (!config.options.onlySpecificNodes)
  {
    return true;
  }

  if (std::find(config.options.dynamicIncludedTypes.begin(),
                config.options.dynamicIncludedTypes.end(),
                node->getTag())
      != config.options.dynamicIncludedTypes.end())
  {
    return true;
  }

  if (std::find(config.options.dynamicExcludedTypes.begin(),
                config.options.dynamicExcludedTypes.end(),
                node->getTag())
      != config.options.dynamicExcludedTypes.end())
  {
    return false;
  }

  std::vector<std::string> supertypes
      = getSupertypes(config.options.nodeTypes, node->getTag());
  supertypes.push_back(node->getTag());

  // check if one of the supertypes is in the
  // config.onlySpecificNodes.nodeTypes list

  for (const auto &nodeType : config.options.onlySpecificNodes->nodeTypes)
  {
    if (std::find(supertypes.begin(), supertypes.end(), nodeType)
        != supertypes.end())
    {
      if (config.options.debug)
      {
        std::cout << "Type " << node->getTag() << " is included" << std::endl;
      }
      config.options.dynamicIncludedTypes.push_back(node->getTag());
      return true;
    }
  }
  if (config.options.debug)
  {
    std::cout << "Type " << node->getTag() << " is not included" << std::endl;
  }
  config.options.dynamicExcludedTypes.push_back(node->getTag());
  return false;
}

#endif
