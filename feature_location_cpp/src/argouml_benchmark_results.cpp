#include "argouml_benchmark_results.hpp"
#include "configuration.hpp"
#include "set_operations.hpp"
#include <filesystem>
#include <vector>

const std::string refinementSuffix { "Refinement" };

enum class TraceExtent
{
  none,
  refinement,
  full
};

TraceExtent isTrace(const std::shared_ptr<Node>              &node,
                     const std::vector<std::shared_ptr<Node>> &otherNodes)
{
  auto result { TraceExtent::full };
  for (const auto &otherNode : otherNodes)
  {
    if (node.get() == otherNode.get())
    {
      return TraceExtent::none;
    }
    if (otherNode->isDescendant(node))
    {
      return TraceExtent::none;
    }
    if (node->isDescendant(otherNode))
    {
      result = TraceExtent::refinement;
    }
  }
  return result;
}

bool isMethodDeclaration(const std::shared_ptr<Node> &node)
{
  return node->getTag() == "method_declaration"
         || node->getTag() == "constructor_declaration";
}

bool isClassDeclaration(const std::shared_ptr<Node> &node)
{
  return node->getTag() == "class_declaration";
}

std::string getIdentifier(const std::shared_ptr<Node> &node)
{
  auto identifier { node->getChildByTag("identifier") };
  if (identifier == nullptr)
  {
    return "";
  }
  return identifier->getTsText();
}

std::string getClassOrMethodIdentifier(std::shared_ptr<Node> node)
{
  while (node != nullptr && !isClassDeclaration(node)
         && !isMethodDeclaration(node))
  {
    node = node->getParent();
  }
  if (node == nullptr)
  {
    return "";
  }
  return getIdentifier(node);
}

std::string getClassFqn(std::shared_ptr<Node> node)
{
  if (!isClassDeclaration(node))
  {
    return "";
  }
  auto identifier { getIdentifier(node) };

  // Get to the program node
  while (node != nullptr && node->getTag() != "program")
  {
    node = node->getParent();
  }
  if (node == nullptr)
  {
    return identifier;
  }

  // Find package declaration
  auto packageDeclaration = node->getChildByTag("package_declaration");
  if (packageDeclaration == nullptr)
  {
    return identifier;
  }

  // Get all identifiers from package declaration
  std::vector<std::string> packageParts;
  auto                     current = packageDeclaration;
  while (current != nullptr)
  {
    auto children { current->getChildren() };
    std::vector<std::string>
        thisLevelParts; // use intermediate vector, because of the order
    for (const auto &child : children)
    {
      if (child->getTag() == "identifier")
      {
        thisLevelParts.push_back(child->getTsText());
      }
    }
    packageParts.insert(packageParts.begin(),
                         thisLevelParts.begin(),
                         thisLevelParts.end());
    current = current->getChildByTag("scoped_identifier");
  }

  // Combine package parts with dots
  std::string packageName;
  for (size_t i = 0; i < packageParts.size(); ++i)
  {
    if (i > 0)
    {
      packageName += ".";
    }
    packageName += packageParts[i];
  }

  // Return full package name + class name
  return packageName.empty() ? identifier : packageName + "." + identifier;
}

/**
 * Gets the fully qualified name of a method node.
 *
 * @param node The AST node representing a method_declaration
 * @return The fully qualified method name including class and parameters, or
 * empty string if not a method_declaration
 */
std::string getMethodFqn(std::shared_ptr<Node> node)
{
  if (!isMethodDeclaration(node))
  {
    return "";
  }
  auto identifier { getIdentifier(node) };

  // Get to the class declaration
  auto classNode = node;
  while (classNode != nullptr && !isClassDeclaration(classNode))
  {
    classNode = classNode->getParent();
  }
  if (classNode == nullptr)
  {
    return "";
  }

  // Get the class FQN
  std::string classFqn = getClassFqn(classNode);
  if (classFqn.empty())
  {
    return "";
  }

  // Get parameter types
  std::vector<std::string> paramTypes;
  auto formalParams = node->getChildByTag("formal_parameters");
  if (formalParams != nullptr)
  {
    for (const auto &child : formalParams->getChildren())
    {
      if (child->getTag() == "formal_parameter")
      {
        auto typeIdentifier = child->getChildByTag("type_identifier");
        if (typeIdentifier != nullptr)
        {
          paramTypes.push_back(typeIdentifier->getTsText());
        }
      }
    }
  }

  // Build the method FQN
  std::string methodFqn = classFqn + " " + identifier + "(";
  for (size_t i = 0; i < paramTypes.size(); ++i)
  {
    if (i > 0)
    {
      methodFqn += ", ";
    }
    methodFqn += paramTypes[i];
  }
  methodFqn += ")";

  return methodFqn;
}

std::string getFqn(std::shared_ptr<Node> node)
{
  auto current = node;
  while (current != nullptr)
  {
    if (isMethodDeclaration(current))
    {
      return getMethodFqn(current);
    }
    if (isClassDeclaration(current))
    {
      return getClassFqn(current);
    }
    current = current->getParent();
  }
  return "";
}

struct OutputLine
{
    std::string identifier;
    bool        isRefinement;
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
      for (const auto &line : other.getLines())
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
                  + (line.isRefinement ? " " + refinementSuffix : "") + "\n";
      }
      return output;
    }

    const std::vector<OutputLine> &getLines() const
    {
      return lines;
    }
  private:
    std::vector<OutputLine> lines;
};

OutputLines findFullTraces(
    const std::vector<std::shared_ptr<Node>> &nodes,
    const std::vector<std::shared_ptr<Node>> &subtractionNodes)
{
  OutputLines outputLines;
  for (const auto &node : nodes)
  {
    // Check if node is full class trace
    if (isClassDeclaration(node)
        && isTrace(node, subtractionNodes) == TraceExtent::full)
    {
      std::string classIdentifier { getFqn(node) };
      if (!classIdentifier.empty())
      {
        outputLines.insert({ classIdentifier, false });
      }
    }
    // Check if node is full method trace
    else if (isMethodDeclaration(node)
             && isTrace(node, subtractionNodes) == TraceExtent::full)
    {
      std::string methodIdentifier { getFqn(node) };
      if (!methodIdentifier.empty())
      {
        outputLines.insert({ methodIdentifier, false });
      }
    }
    // Recursively check children
    else
    {
      auto childTraces
          = findFullTraces(node->getChildren(), subtractionNodes);
      // Merge child traces into output lines
      outputLines.insertMany(childTraces);
    }
  }
  return outputLines;
}

OutputLines findRefinementTraces(
    const std::vector<std::shared_ptr<Node>> &nodes,
    const std::vector<std::shared_ptr<Node>> &subtractionNodes)
{
  OutputLines outputLines;
  for (const auto &node : nodes)
  {
    for (const auto &maybeLeaf : node->getPointerToEveryNode())
    {
      if (maybeLeaf->isLeaf())
      {
        const auto &leaf { maybeLeaf };
        auto        traceExtent { isTrace(leaf, subtractionNodes) };
        if (traceExtent == TraceExtent::refinement
            || traceExtent == TraceExtent::full)
        {
          auto ancestorIdentifier { getFqn(leaf) };
          if (!ancestorIdentifier.empty())
          {
            outputLines.insert({ ancestorIdentifier, true });
          }
        }
      }
    }
  }
  return outputLines;
}

std::string
    buildArgoumlBenchmarkOutputForFile(DifferenceResult differenceResult,
                                            Configuration    config)
{
  if (config.options.language != "java")
  {
    throw std::runtime_error("Language is not java");
  }

  const auto &fileDifferenceResult { differenceResult.result[0] };

  OutputLines fullTraceOutputLines { findFullTraces(
      fileDifferenceResult.intersection,
      fileDifferenceResult.subtraction) };

  OutputLines refinementOutputLines { findRefinementTraces(
      fileDifferenceResult.intersection,
      fileDifferenceResult.subtraction) };

  fullTraceOutputLines.insertMany(refinementOutputLines);

  return fullTraceOutputLines.render();
}

// TODO: Add support for nested classes/methods, currently they are handled
// wrong
std::string buildArgoumlBenchmarkOutput(
    std::vector<DifferenceResult> differenceResults, Configuration config)
{
  std::string output;
  for (const auto &differenceResult : differenceResults)
  {
    output
        += buildArgoumlBenchmarkOutputForFile(differenceResult, config);
  }
  return output;
}
