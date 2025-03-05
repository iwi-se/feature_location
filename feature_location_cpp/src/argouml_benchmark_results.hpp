#ifndef ARGOUML_BENCHMARK_RESULTS_HPP
#define ARGOUML_BENCHMARK_RESULTS_HPP

#include <vector>
#include <filesystem>
#include "configuration.hpp"
#include "set_operations.hpp"

bool is_trace(const std::shared_ptr<Node> &node, const std::vector<std::shared_ptr<Node>> &other_nodes)
{
    auto result {true};
    for (const auto &other_node : other_nodes)
    {
        if (node.get() == other_node.get() || node->is_descendant(other_node))
        {
            std::cout << "No trace: " << node->get_tag() << " is descendant of " << other_node->get_tag() << std::endl;
            return false;
        }
    }
    return result;
}

std::string get_identifier(const std::shared_ptr<Node> &node)
{
    auto identifier {node->get_child_by_tag("identifier")};
    if (identifier == nullptr)
    {
        return "";
    }
    return identifier->get_ts_text();
}

std::string get_class_or_method_identifier(std::shared_ptr<Node> node)
{
    while (node != nullptr && node->get_tag() != "class_declaration" && node->get_tag() != "method_declaration")
    {
        node = node->get_parent();
    }
    if (node == nullptr)
    {
        return "";
    }
    return get_identifier(node);
}


std::string build_argouml_benchmark_output_for_file(DifferenceResult difference_result, Configuration config)
{
    if (config.options.language != "java")
    {
        throw std::runtime_error("Language is not java");
    }
    std::string output;
    const auto &file_difference_result {difference_result.result[0]};
    for (const auto &intersection_node : file_difference_result.intersection)
    {
        if (is_trace(intersection_node, file_difference_result.subtraction))
        {
            if (config.options.debug)
            {
                std::cout << "Found trace: " << intersection_node->get_tag() << std::endl;
            }
            if (intersection_node->get_tag() == "class_declaration")
            {
                std::string ancestor_identifier {get_identifier(intersection_node)}; // supposed to return class_delcaration or method_declaration, to decide for class refinement trace or method refinement trace                   
                output += ancestor_identifier + "\n";
            }
            else if (intersection_node->get_tag() == "method_declaration")
            {
                std::string ancestor_identifier {get_identifier(intersection_node)}; // supposed to return class_delcaration or method_declaration, to decide for class refinement trace or method refinement trace                   
                output += ancestor_identifier + "\n";
            }
            else
            {
                std::string ancestor_identifier {get_class_or_method_identifier(intersection_node)}; // supposed to return class_delcaration or method_declaration, to decide for class refinement trace or method refinement trace                   
                if (!ancestor_identifier.empty())
                {
                    output += ancestor_identifier + " refinement trace\n";
                }
            }
        }
    }
        /* Pseudo code */
        /* for each node that is part of intersection, check first that it is not part of the subtraction
            if not, check if it is a class_declaration, field_declaration, method_declaration
                if yes, add it to the output (if class->class trace, if field->class refinement trace (must find class first), if method->method trace)
                if no, check if any of the parents of the node is a class_declaration, field_declaration, method_declaration
                    if yes, add it to the output (if class->class refinement trace, if field->class refinement trace (must find class first), if method->method refinement trace)
         */
    return output;
}

std::string build_argouml_benchmark_output(std::vector<DifferenceResult> difference_results, Configuration config)
{
    std::string output;
    for (const auto &difference_result : difference_results)
    {
        output += build_argouml_benchmark_output_for_file(difference_result, config);
    }
    return output;
}

#endif