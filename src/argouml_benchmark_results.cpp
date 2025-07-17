#include "argouml_benchmark_results.hpp"
#include "configuration.hpp"
#include "node_types.hpp"
#include "set_operations.hpp"
#include <set>
#include <stack>
#include <vector>

const std::string refinementSuffix { "Refinement" };

enum class TraceExtent
{
  none,
  refinement,
  full
};

TraceExtent isTrace(Node *node, const std::vector<Node *> &otherNodes)
{
  auto result { TraceExtent::full };
  for (const auto &otherNode : otherNodes)
  {
    if (node == otherNode)
    {
      return TraceExtent::none;
    }
    if (otherNode->isAncestorOf(node))
    {
      return TraceExtent::none;
    }
    if (node->isAncestorOf(otherNode))
    {
      result = TraceExtent::refinement;
    }
  }
  return result;
}

bool isMethodDeclaration(Node *node)
{
  return node != nullptr
         && (node->getTag() == "method_declaration"
             || node->getTag() == "constructor_declaration");
}

bool isClassDeclaration(Node *node)
{
  return node != nullptr
         && (node->getTag() == "class_declaration"
             || node->getTag() == "interface_declaration");
}

bool isImportDeclaration(Node *node)
{
  return node != nullptr && node->getTag() == "import_declaration";
}

std::string getIdentifier(Node *node)
{
  auto identifier { node->getChildByTag("identifier") };
  if (identifier == nullptr)
  {
    return "";
  }
  return identifier->getTsText();
}

Node *getParentMethodNode(Node *node)
{
  while (node != nullptr && !isMethodDeclaration(node))
  {
    node = node->getParent();
  }
  return node;
}

Node *getParentClassNode(Node *node)
{
  while (node != nullptr && !isClassDeclaration(node))
  {
    node = node->getParent();
  }
  return node;
}

std::vector<Node *> getTopLevelClassNodes(Node *node)
{
  std::vector<Node *> result;
  std::stack<Node *>  stack;
  stack.push(node);
  while (!stack.empty())
  {
    auto current { stack.top() };
    stack.pop();
    if (isClassDeclaration(current))
    {
      result.push_back(current);
    }
    else
    {
      for (const auto &child : current->getChildren())
      {
        stack.push(child.get());
      }
    }
  }
  return result;
}

std::string getClassFqn(Node *node)
{
  if (!isClassDeclaration(node))
  {
    return "";
  }
  // Get to the program node
  std::vector<std::string> classFqns;
  while (node != nullptr && node->getTag() != "program")
  {
    if (isClassDeclaration(node))
    {
      classFqns.push_back(getIdentifier(node));
    }
    node = node->getParent();
  }

  std::string identifier {};
  for (auto it = classFqns.rbegin(); it != classFqns.rend(); ++it)
  {
    if (it != classFqns.rbegin())
    {
      identifier += ".";
    }
    identifier += *it;
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
    auto &children { current->getChildren() };
    std::vector<std::string>
        thisLevelParts; // use intermediate vector, because of the order
    for (const auto &child : children)
    {
      if (child->getTag() == "identifier")
      {
        thisLevelParts.push_back(child->getTsText());
      }
    }
    packageParts.insert(
        packageParts.begin(), thisLevelParts.begin(), thisLevelParts.end());
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
std::string getMethodFqn(Node *node)
{
  if (!isMethodDeclaration(node))
  {
    return "";
  }
  auto identifier { getIdentifier(node) };

  // Get parameter types
  std::vector<std::string> paramTypes;
  auto formalParams = node->getChildByTag("formal_parameters");
  if (formalParams != nullptr)
  {
    for (const auto &child : formalParams->getChildren())
    {
      if (child->getTag() == "formal_parameter")
      {
        auto typeIdentifier = child->getChildren()[0].get();
        if (typeIdentifier->getTag() == "modifiers")
        {
          typeIdentifier = child->getChildren()[1]
                               .get(); // index 0 might be modifiers, then
                                       // type identifier is index 1
        }
        if (typeIdentifier != nullptr)
        {
          paramTypes.push_back(typeIdentifier->getTsText());
        }
      }
    }
  }

  // Build the method FQN
  std::string methodFqn = identifier + "(";
  for (size_t i = 0; i < paramTypes.size(); ++i)
  {
    if (i > 0)
    {
      methodFqn += ",";
    }
    paramTypes[i].erase(
        std::remove(paramTypes[i].begin(), paramTypes[i].end(), ' '),
        paramTypes[i].end());
    methodFqn += paramTypes[i];
  }
  methodFqn += ")";
  // Remove all spaces from the method FQN

  return methodFqn;
}

struct OutputLine
{
    bool isClassLine() const
    {
      return methodFqn.empty() && !isRefinement;
    }

    bool isMethodLine() const
    {
      return !classFqn.empty() && !methodFqn.empty() && !isRefinement;
    }

    std::string classFqn;
    std::string methodFqn;
    bool        isRefinement;
};

bool operator== (const OutputLine &a, const OutputLine &b)
{
  return a.classFqn == b.classFqn && a.methodFqn == b.methodFqn
         && a.isRefinement == b.isRefinement;
}

bool operator< (const OutputLine &a, const OutputLine &b)
{
  return a.classFqn < b.classFqn
         || (a.classFqn == b.classFqn && a.methodFqn < b.methodFqn);
}

class OutputLines
{
  public:
    void insert(const OutputLine &line)
    {
      if (line.isClassLine())
      {
        classLines.insert(line);
      }
      else if (line.isMethodLine())
      {
        methodLines.insert(line);
      }
      else
      {
        refinementLines.insert(line);
      }
    }

    void insertMany(const OutputLines &other)
    {
      for (const auto &line : other.classLines)
      {
        insert(line);
      }
      for (const auto &line : other.methodLines)
      {
        insert(line);
      }
      for (const auto &line : other.refinementLines)
      {
        insert(line);
      }
    }

    void removeSuperfluousLines()
    {
      for (const auto &classLine : classLines)
      {
        std::erase_if(methodLines,
                      [&classLine](const OutputLine &line)
                      {
                        return line.classFqn.starts_with(classLine.classFqn
                                                         + ".")
                               || line.classFqn == classLine.classFqn;
                      });
        std::erase_if(refinementLines,
                      [&classLine](const OutputLine &line)
                      {
                        return line.classFqn.starts_with(classLine.classFqn
                                                         + ".")
                               || line.classFqn == classLine.classFqn;
                      });
      }

      for (const auto &methodLine : methodLines)
      {
        std::erase_if(refinementLines,
                      [&methodLine](const OutputLine &line)
                      {
                        return line.classFqn == methodLine.classFqn
                               && line.methodFqn == methodLine.methodFqn;
                      });
      }
    }

    std::string render()
    {
      removeSuperfluousLines();
      std::string output;
      for (auto &line : classLines)
      {
        output += line.classFqn + "\n";
      }
      for (const auto &line : methodLines)
      {
        output += line.classFqn + " " + line.methodFqn + "\n";
      }
      for (const auto &line : refinementLines)
      {
        output += line.classFqn
                  + (line.methodFqn.empty() ? "" : " " + line.methodFqn) + " "
                  + refinementSuffix + "\n";
      }

      return output;
    }

    std::set<OutputLine> classLines;
    std::set<OutputLine> methodLines;
    std::set<OutputLine> refinementLines;
};

std::vector<Node *> findAllClassNodes(Node *root)
{
  std::vector<Node *> classNodes;
  std::stack<Node *>  stack;
  stack.push(root);
  while (!stack.empty())
  {
    auto current { stack.top() };
    stack.pop();
    if (isClassDeclaration(current))
    {
      classNodes.push_back(current);
    }
    for (const auto &child : current->getChildren())
    {
      stack.push(child.get());
    }
  }
  return classNodes;
}

std::vector<Node *> findAllMethodNodes(Node *root)
{
  std::vector<Node *> classNodes;
  std::stack<Node *>  stack;
  stack.push(root);
  while (!stack.empty())
  {
    auto current { stack.top() };
    stack.pop();
    if (isMethodDeclaration(current))
    {
      classNodes.push_back(current);
    }
    for (const auto &child : current->getChildren())
    {
      stack.push(child.get());
    }
  }
  return classNodes;
}

std::vector<Node *> findAllImportNodes(Node *root)
{
  std::vector<Node *> classNodes;
  std::stack<Node *>  stack;
  stack.push(root);
  while (!stack.empty())
  {
    auto current { stack.top() };
    stack.pop();
    if (isImportDeclaration(current))
    {
      classNodes.push_back(current);
    }
    for (const auto &child : current->getChildren())
    {
      stack.push(child.get());
    }
  }
  return classNodes;
}

OutputLines findFullTraces(const std::vector<Node *> &nodes,
                           const std::vector<Node *> &subtractionNodes)
{
  OutputLines outputLines;
  // Check if node is full class trace
  auto classNodes { findAllClassNodes(nodes[0]->getRoot()) };

  for (const auto &classNode : classNodes)
  {
    bool allLeavesFullTrace { true };
    for (auto leave : classNode->getLeafs())
    {
      if (isTrace(leave, subtractionNodes) != TraceExtent::full)
      {
        allLeavesFullTrace = false;
        break;
      }
    }
    if (allLeavesFullTrace)
    {
      auto classIdentifier { getClassFqn(classNode) };
      outputLines.insert({ classIdentifier, "", false });
    }
  }

  auto methodNodes { findAllMethodNodes(nodes[0]->getRoot()) };

  for (const auto &methodNode : methodNodes)
  {
    bool allLeavesFullTrace { true };
    for (auto leave : methodNode->getLeafs())
    {
      if (isTrace(leave, subtractionNodes) != TraceExtent::full)
      {
        allLeavesFullTrace = false;
        break;
      }
    }
    if (allLeavesFullTrace)
    {
      auto classIdentifier { getClassFqn(getParentClassNode(methodNode)) };
      auto methodIdentifier { getMethodFqn(methodNode) };
      outputLines.insert({ classIdentifier, methodIdentifier, false });
    }
  }

  return outputLines;
}

void checkForImpreciseClassTraces(const std::vector<Node *> &nodes,
                                  const std::vector<Node *> &subtractionNodes,
                                  OutputLines               &outputLines)
{
  std::vector<Node *> leftClassNodes {};
  std::vector<Node *> rightClassNodes {};
  if (!nodes.empty())
  {
    leftClassNodes = findAllClassNodes(nodes[0]->getRoot());
  }
  if (!subtractionNodes.empty())
  {
    rightClassNodes = findAllClassNodes(subtractionNodes[0]->getRoot());
  }
  // Find all class names

  for (const auto &leftClassNode : leftClassNodes)
  {
    bool hasTraceInClass { false };
    for (const auto &node : nodes)
    {
      if (leftClassNode->isAncestorOf(node))
      {
        hasTraceInClass = true;
        break;
      }
    }
    if (hasTraceInClass)
    {
      bool isClassNameInSubtraction { false };
      for (const auto &rightClassNode : rightClassNodes)
      {
        if (getClassFqn(rightClassNode) == getClassFqn(leftClassNode))
        {
          isClassNameInSubtraction = true;
          break;
        }
      }
      if (!isClassNameInSubtraction)
      {
        outputLines.insert({ getClassFqn(leftClassNode), "", false });
      }
    }
  }
}

OutputLines findRefinementTraces(const std::vector<Node *> &nodes,
                                 const std::vector<Node *> &subtractionNodes,
                                 Configuration             &config)
{
  OutputLines outputLines;

  auto importDeclarations { findAllImportNodes(nodes[0]->getRoot()) };
  for (auto &importDeclaration : importDeclarations)
  {
    auto leaves { importDeclaration->getLeafs() };
    bool isTraceL { false };
    for (auto &leave : leaves)
    {
      if (isTrace(leave, subtractionNodes) != TraceExtent::none)
      {
        isTraceL = true;
        break;
      }
    }
    if (isTraceL)
    {
      auto classNodes { getTopLevelClassNodes(nodes[0]->getRoot()) };

      for (const auto &classNode : classNodes)
      {
        outputLines.insert({ getClassFqn(classNode), "", true });
      }
      break;
    }
  }

  for (const auto &node : nodes)
  {
    for (auto &includedNode : node->getPointerToEveryNode())
    {
      if (isIncludedNodeType(includedNode, config)
          && !isClassDeclaration(includedNode)
          && !isMethodDeclaration(includedNode))
      {
        auto traceExtent { isTrace(includedNode, subtractionNodes) };
        if (traceExtent == TraceExtent::refinement
            || traceExtent == TraceExtent::full)
        {
          auto methodNode { getParentMethodNode(includedNode) };
          auto classNode { getParentClassNode(includedNode) };

          std::string methodIdentifier {};
          if (methodNode != nullptr)
          {
            methodIdentifier = getMethodFqn(methodNode);
          }
          std::string classIdentifier { getClassFqn(classNode) };
          if (!classIdentifier.empty() || !methodIdentifier.empty())
          {
            outputLines.insert({ classIdentifier, methodIdentifier, true });
          }
        }
      }
    }
  }
  checkForImpreciseClassTraces(nodes, subtractionNodes, outputLines);
  return outputLines;
}

std::string
    buildArgoumlBenchmarkOutputForFile(DifferenceResult differenceResult,
                                       Configuration   &config)
{
  if (config.options.language != "java")
  {
    throw std::runtime_error("Language is not java");
  }

  const auto &fileDifferenceResult { differenceResult.result[0] };

  if (fileDifferenceResult.intersection.empty()
      && fileDifferenceResult.subtraction.empty())
  {
    return "";
  }

  OutputLines fullTraceOutputLines { findFullTraces(
      fileDifferenceResult.intersection, fileDifferenceResult.subtraction) };

  OutputLines refinementOutputLines { findRefinementTraces(
      fileDifferenceResult.intersection,
      fileDifferenceResult.subtraction,
      config) };

  fullTraceOutputLines.insertMany(refinementOutputLines);

  return fullTraceOutputLines.render();
}

// TODO: Add support for nested classes/methods, currently they are handled
// wrong
std::string
    buildArgoumlBenchmarkOutput(std::vector<DifferenceResult> differenceResults,
                                Configuration                &config)
{
  std::string output;
  for (const auto &differenceResult : differenceResults)
  {
    output += buildArgoumlBenchmarkOutputForFile(differenceResult, config);
  }
  return output;
}

// Full class trace:
// -> get root, get all class nodes, check for each class nodes if every
// leave/token is in result Full method trace:
// -> get root, get all method nodes, check for each method node if every
// leave/token is in result
