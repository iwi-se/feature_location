#include "argouml_benchmark_results.hpp"
#include "render.hpp"
#include "configuration.hpp"
#include "evaluation.hpp"
#include "expression.hpp"
#include "tree.hpp"
#include <fstream>
#include <iostream>
#include <vector>

void renderResultsToFiles(std::vector<DifferenceResult> differenceResults,
                          Configuration                 config)
{
  for (const auto &differenceResult : differenceResults)
  {
    std::string htmlResult = renderDifference(differenceResult, config);
    auto        relPathString { differenceResult.relativePath.string() };
    std::replace(relPathString.begin(), relPathString.end(), '/', '_');
    std::string   outputFileName = "difference_" + relPathString + ".html";
    std::ofstream outputFile(outputFileName);
    outputFile << htmlResult;
    outputFile.close();
  }
}

void renderArgoumlBenchmarkResultsToFiles(
    std::vector<DifferenceResult> differenceResults, Configuration config)
{
  std::string result { buildArgoumlBenchmarkOutput(differenceResults, config) };
  std::ofstream outputFile("argouml_benchmark_results.txt");
  outputFile << result;
  outputFile.close();
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <config.yaml>" << std::endl;
    return 1;
  }
  std::string                       configFile = argv[1];
  Configuration                     config(configFile);
  std::vector<ExpressionSystemName> expressions
      = config.getExpressionsToEvaluate();
  if (config.options.debug)
  {
    config.render();
  }

  for (const auto &expression : expressions)
  {
    auto differenceResults = runExpression(expression, config);
    renderResultsToFiles(differenceResults, config);
    if (config.options.language == "java")
    {
      renderArgoumlBenchmarkResultsToFiles(differenceResults, config);
    }
  }
  return 0;
}
