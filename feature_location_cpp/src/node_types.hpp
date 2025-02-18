#ifndef NODE_TYPES_HPP
#define NODE_TYPES_HPP

#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stack>

using json = nlohmann::json;

json node_types;

std::vector<std::string> get_supertypes(const std::filesystem::path &node_types_file, const std::string &type)
{
    std::vector<std::string> supertypes;

    // Read and parse JSON file
    if (node_types.empty())
    {
        try
        {
            std::ifstream f(node_types_file);
            node_types = json::parse(f);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing node types file: " << e.what() << std::endl;
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

        for (const auto &node_type : node_types)
        {
            if (node_type.contains("subtypes"))
            {
                for (const auto &subtype : node_type["subtypes"])
                {
                    if (subtype["type"] == current)
                    {
                        supertypes.push_back(node_type["type"]);
                        stack.push(node_type["type"]);
                    }
                }
            }
        }
    }

    return supertypes;
}

bool is_included_node_type(const std::filesystem::path &node_types_file, const std::string &type, const Configuration &config)
{
    std::vector<std::string> supertypes = get_supertypes(node_types_file, type);
    supertypes.push_back(type);
    // check if one of the supertypes is in the config.only_specific_nodes.node_types list

    for (const auto &node_type : config.options.only_specific_nodes.node_types)
    {
        if (std::find(supertypes.begin(), supertypes.end(), node_type) != supertypes.end())
        {
            if (config.options.debug)
            {
                std::cout << "Type " << type << " is included" << std::endl;
            }
            return true;
        }
    }
    if (config.options.debug)
    {
        std::cout << "Type " << type << " is not included" << std::endl;
    }
    return false;
}

#endif