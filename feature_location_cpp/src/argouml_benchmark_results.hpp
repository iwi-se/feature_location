#ifndef ARGOUML_BENCHMARK_RESULTS_HPP
#define ARGOUML_BENCHMARK_RESULTS_HPP

#include "configuration.hpp"
#include "set_operations.hpp"
#include <filesystem>
#include <vector>

const std::string refinement_suffix { "Refinement" };

enum class TraceExtent
{
  none,
  refinement,
  full
};

TraceExtent is_trace(const std::shared_ptr<Node>              &node,
                     const std::vector<std::shared_ptr<Node>> &other_nodes)
{
  auto result { TraceExtent::full };
  for (const auto &other_node : other_nodes)
  {
    if (node.get() == other_node.get())
    {
      return TraceExtent::none;
    }
    if (other_node->is_descendant(node))
    {
      std::cout << "No trace: " << node->get_ts_text() << " is descendant of "
                << other_node->get_ts_text() << std::endl;
      return TraceExtent::none;
    }
    if (node->is_descendant(other_node))
    {
      result = TraceExtent::refinement;
    }
  }
  return result;
}

std::string get_identifier(const std::shared_ptr<Node> &node)
{
  auto identifier { node->get_child_by_tag("identifier") };
  if (identifier == nullptr)
  {
    return "";
  }
  return identifier->get_ts_text();
}

std::string get_class_or_method_identifier(std::shared_ptr<Node> node)
{
  while (node != nullptr && node->get_tag() != "class_declaration"
         && node->get_tag() != "method_declaration")
  {
    node = node->get_parent();
  }
  if (node == nullptr)
  {
    return "";
  }
  return get_identifier(node);
}

std::string get_method_fqn(std::shared_ptr<Node> node)
{
  if (node->get_tag() != "method_declaration")
  {
    return "";
  }
  auto identifier { get_identifier(node) };
  // TODO: get the class node and then get the class fqn
  // Pseudocode:
  // * Get to the parent node with tag "class_declaration"
  // * Get the class fqn (use get_class_fqn)

  // * Get the child of method node with tag "formal_parameters"
  // * Get the children of formal_parameters with tag "formal_parameter"
  // * Get the node with tag "type_identifier" of the formal_parameter
  // * Get the identifier of the type_identifier with ts_text

  // * Combine the class fqn and the method identifier with "." in between
  // * append the parameter types, comma separated and enclosed in parentheses (Type, Type, Type)
  // * return the result
}

std::string get_class_fqn(std::shared_ptr<Node> node)
{
  if (node->get_tag() != "class_declaration")
  {
    return "";
  }
  auto identifier { get_identifier(node) };
  // TODO: get the package name
  // Pseudocode:
  // 1. Get to the parent node with tag "program"
  // 2. Go to first child node with tag "package_declaration"
  // 3. Get all children with tag "identifier"
  // 4. Combine all identifiers with "." in between
  // 5. Return the fqn
}

std::string get_fqn(std::shared_ptr<Node> node)
{
  if (node->get_tag() == "method_declaration")
  {
    return get_method_fqn(node);
  }
  if (node->get_tag() == "class_declaration")
  {
    return get_class_fqn(node);
  }
  return "";
}

std::string
    build_argouml_benchmark_output_for_file(DifferenceResult difference_result,
                                            Configuration    config)
{
  if (config.options.language != "java")
  {
    throw std::runtime_error("Language is not java");
  }
  std::string output;
  const auto &file_difference_result { difference_result.result[0] };
  for (const auto &intersection_node : file_difference_result.intersection)
  {
    const auto trace_extent { is_trace(intersection_node,
                                       file_difference_result.subtraction) };
    if (trace_extent == TraceExtent::full
        || trace_extent == TraceExtent::refinement)
    {
      if (config.options.debug)
      {
        std::cout << "Found trace: " << intersection_node->get_tag()
                  << std::endl;
      }
      if (intersection_node->get_tag() == "class_declaration")
      {
        std::string ancestor_identifier {
          get_identifier(intersection_node)
        };
        if (trace_extent == TraceExtent::refinement)
        {
          output += ancestor_identifier + refinement_suffix + "\n";
        }
        else
        {
          output += ancestor_identifier + "\n";
        }
      }
      else if (intersection_node->get_tag() == "method_declaration")
      {
        std::string ancestor_identifier {
          get_identifier(intersection_node)
        };
        if (trace_extent == TraceExtent::refinement)
        {
          output += ancestor_identifier + "() " + refinement_suffix + "\n";
        }
        else
        {
          output += ancestor_identifier + "()\n";
        }
      }
      else
      {
        std::string ancestor_identifier {
          get_class_or_method_identifier(intersection_node)
        }; // supposed to return class_declaration or method_declaration, to
           // decide for class refinement trace or method refinement trace
        if (!ancestor_identifier.empty())
        {
          output += ancestor_identifier + refinement_suffix + "\n";
        }
      }
    }
  }
  /* Pseudo code */
  /* for each node that is part of intersection, check first that it is not part
     of the subtraction if not, check if it is a class_declaration,
     field_declaration, method_declaration if yes, add it to the output (if
     class->class trace, if field->class refinement trace (must find class
     first), if method->method trace) if no, check if any of the parents of the
     node is a class_declaration, field_declaration, method_declaration if yes,
     add it to the output (if class->class refinement trace, if field->class
     refinement trace (must find class first), if method->method refinement
     trace)
   */
  return output;
}

std::string build_argouml_benchmark_output(
    std::vector<DifferenceResult> difference_results, Configuration config)
{
  std::string output;
  for (const auto &difference_result : difference_results)
  {
    output
        += build_argouml_benchmark_output_for_file(difference_result, config);
  }
  return output;
}

#endif
