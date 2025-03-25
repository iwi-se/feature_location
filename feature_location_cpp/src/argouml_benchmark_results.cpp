#include "argouml_benchmark_results.hpp"
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
      return TraceExtent::none;
    }
    if (node->is_descendant(other_node))
    {
      result = TraceExtent::refinement;
    }
  }
  return result;
}

bool is_method_declaration(const std::shared_ptr<Node> &node)
{
  return node->get_tag() == "method_declaration"
         || node->get_tag() == "constructor_declaration";
}

bool is_class_declaration(const std::shared_ptr<Node> &node)
{
  return node->get_tag() == "class_declaration";
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
  while (node != nullptr && !is_class_declaration(node)
         && !is_method_declaration(node))
  {
    node = node->get_parent();
  }
  if (node == nullptr)
  {
    return "";
  }
  return get_identifier(node);
}

std::string get_class_fqn(std::shared_ptr<Node> node)
{
  if (!is_class_declaration(node))
  {
    return "";
  }
  auto identifier { get_identifier(node) };

  // Get to the program node
  while (node != nullptr && node->get_tag() != "program")
  {
    node = node->get_parent();
  }
  if (node == nullptr)
  {
    return identifier;
  }

  // Find package declaration
  auto package_declaration = node->get_child_by_tag("package_declaration");
  if (package_declaration == nullptr)
  {
    return identifier;
  }

  // Get all identifiers from package declaration
  std::vector<std::string> package_parts;
  auto                     current = package_declaration;
  while (current != nullptr)
  {
    auto children { current->get_children() };
    std::vector<std::string> this_level_parts; // use intermediate vector, because of the order
    for (const auto &child : children)
    {
      if (child->get_tag() == "identifier")
      {
        this_level_parts.push_back(child->get_ts_text());
      }
    }
    package_parts.insert(package_parts.begin(), this_level_parts.begin(), this_level_parts.end());
    current = current->get_child_by_tag("scoped_identifier");
  }

  // Combine package parts with dots
  std::string package_name;
  for (size_t i = 0; i < package_parts.size(); ++i)
  {
    if (i > 0)
    {
      package_name += ".";
    }
    package_name += package_parts[i];
  }

  // Return full package name + class name
  return package_name.empty() ? identifier : package_name + "." + identifier;
}

/**
 * Gets the fully qualified name of a method node.
 *
 * @param node The AST node representing a method_declaration
 * @return The fully qualified method name including class and parameters, or
 * empty string if not a method_declaration
 */
std::string get_method_fqn(std::shared_ptr<Node> node)
{
  if (!is_method_declaration(node))
  {
    return "";
  }
  auto identifier { get_identifier(node) };

  // Get to the class declaration
  auto class_node = node;
  while (class_node != nullptr && !is_class_declaration(class_node))
  {
    class_node = class_node->get_parent();
  }
  if (class_node == nullptr)
  {
    return "";
  }

  // Get the class FQN
  std::string class_fqn = get_class_fqn(class_node);
  if (class_fqn.empty())
  {
    return "";
  }

  // Get parameter types
  std::vector<std::string> param_types;
  auto formal_params = node->get_child_by_tag("formal_parameters");
  if (formal_params != nullptr)
  {
    for (const auto &child : formal_params->get_children())
    {
      if (child->get_tag() == "formal_parameter")
      {
        auto type_identifier = child->get_child_by_tag("type_identifier");
        if (type_identifier != nullptr)
        {
          param_types.push_back(type_identifier->get_ts_text());
        }
      }
    }
  }

  // Build the method FQN
  std::string method_fqn = class_fqn + " " + identifier + "(";
  for (size_t i = 0; i < param_types.size(); ++i)
  {
    if (i > 0)
    {
      method_fqn += ", ";
    }
    method_fqn += param_types[i];
  }
  method_fqn += ")";

  return method_fqn;
}

std::string get_fqn(std::shared_ptr<Node> node)
{
  auto current = node;
  while (current != nullptr)
  {
    if (is_method_declaration(current))
    {
      return get_method_fqn(current);
    }
    if (is_class_declaration(current))
    {
      return get_class_fqn(current);
    }
    current = current->get_parent();
  }
  return "";
}

struct OutputLine
{
    std::string identifier;
    bool        is_refinement;
};

bool operator== (const OutputLine &lhs, const OutputLine &rhs)
{
  return lhs.identifier == rhs.identifier;
}

class OutputLines
{
  public:
    void insert(const OutputLine &line)
    {
      if (std::find(lines.begin(), lines.end(), line) == lines.end())
      {
        lines.push_back(line);
      }
    }

    void insertMany(const OutputLines &other)
    {
      for (const auto &line : other.get_lines())
      {
        insert(line);
      }
    }

    std::string render()
    {
      std::string output;
      for (const auto &line : lines)
      {
        output += line.identifier
                  + (line.is_refinement ? " " + refinement_suffix : "") + "\n";
      }
      return output;
    }

    const std::vector<OutputLine> &get_lines() const
    {
      return lines;
    }
  private:
    std::vector<OutputLine> lines;
};

OutputLines find_full_traces(
    const std::vector<std::shared_ptr<Node>> &nodes,
    const std::vector<std::shared_ptr<Node>> &subtraction_nodes)
{
  OutputLines outputLines;
  for (const auto &node : nodes)
  {
    // Check if node is full class trace
    if (is_class_declaration(node)
        && is_trace(node, subtraction_nodes) == TraceExtent::full)
    {
      std::string class_identifier { get_fqn(node) };
      if (!class_identifier.empty())
      {
        outputLines.insert({ class_identifier, false });
      }
    }
    // Check if node is full method trace
    else if (is_method_declaration(node)
             && is_trace(node, subtraction_nodes) == TraceExtent::full)
    {
      std::string method_identifier { get_fqn(node) };
      if (!method_identifier.empty())
      {
        outputLines.insert({ method_identifier, false });
      }
    }
    // Recursively check children
    else
    {
      auto child_traces
          = find_full_traces(node->get_children(), subtraction_nodes);
      // Merge child traces into output lines
      outputLines.insertMany(child_traces);
    }
  }
  return outputLines;
}

OutputLines find_refinement_traces(
    const std::vector<std::shared_ptr<Node>> &nodes,
    const std::vector<std::shared_ptr<Node>> &subtraction_nodes)
{
  OutputLines outputLines;
  for (const auto &node : nodes)
  {
    for (const auto &maybe_leaf : node->get_pointer_to_every_node())
    {
      if (maybe_leaf->is_leaf())
      {
        const auto &leaf { maybe_leaf };
        auto trace_extent { is_trace(leaf, subtraction_nodes) };
        if (trace_extent == TraceExtent::refinement ||
            trace_extent == TraceExtent::full)
        {
          auto ancestor_identifier { get_fqn(leaf) };
          if (!ancestor_identifier.empty())
          {
            outputLines.insert({ ancestor_identifier, true });
          }
        }
      }
    }
  }
  return outputLines;
}

std::string
    build_argouml_benchmark_output_for_file(DifferenceResult difference_result,
                                            Configuration    config)
{
  if (config.options.language != "java")
  {
    throw std::runtime_error("Language is not java");
  }

  const auto &file_difference_result { difference_result.result[0] };

  OutputLines full_trace_output_lines { find_full_traces(
      file_difference_result.intersection,
      file_difference_result.subtraction) };

  OutputLines refinement_output_lines { find_refinement_traces(
      file_difference_result.intersection,
      file_difference_result.subtraction) };

  full_trace_output_lines.insertMany(refinement_output_lines);

  return full_trace_output_lines.render();
}

// TODO: Add support for nested classes/methods, currently they are handled wrong
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
