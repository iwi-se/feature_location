#ifndef NODE_TYPES_HPP
#define NODE_TYPES_HPP

#include <fstream>
#include <nlohmann/json.hpp>
#include <stack>
#include <string>
#include <vector>

using json = nlohmann::json;

json nodeTypes;

std::vector<std::string>
    getSupertypes(const std::filesystem::path &nodeTypesFile,
                  const std::string           &type)
{
  std::vector<std::string> supertypes;

  // Read and parse JSON file
  if (nodeTypes.empty())
  {
    try
    {
      std::ifstream f(nodeTypesFile);
      nodeTypes = json::parse(f);
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error parsing node types file: " << nodeTypesFile << " " << e.what() << std::endl;
      return supertypes;
    }
  }

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

bool isIncludedNodeType(const std::shared_ptr<Node> &node,
                        const Configuration         &config)
{
  if (!config.options.onlySpecificNodes)
  {
    return true;
  }

  auto supertypes = node->getNodeTypes();
  if (supertypes.empty())
  {
    supertypes = getSupertypes(config.options.onlySpecificNodes->nodeTypesFile,
                               node->getTag());
    supertypes.push_back(node->getTag());
    node->setNodeTypes(supertypes);
  }

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
      return true;
    }
  }
  if (config.options.debug)
  {
    std::cout << "Type " << node->getTag() << " is not included" << std::endl;
  }
  return false;
}

#endif
