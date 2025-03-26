#include "argouml_benchmark_results.hpp"
#include "configuration.hpp"
#include "evaluation.hpp"
#include "expression.hpp"
#include "render.hpp"
#include "tree.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

// Ensure the results directory exists
void ensureResultsDirectoryExists()
{
  const fs::path resultsDir = "results";
  if (!fs::exists(resultsDir))
  {
    fs::create_directory(resultsDir);
  }
}

void renderResultsToFiles(std::vector<DifferenceResult> differenceResults,
                          Configuration                 config)
{
  ensureResultsDirectoryExists();

  for (const auto &differenceResult : differenceResults)
  {
    std::string htmlResult = renderDifference(differenceResult, config);
    auto        relPathString { differenceResult.relativePath.string() };
    std::replace(relPathString.begin(), relPathString.end(), '/', '_');
    fs::path outputFileName
        = fs::path("results") / ("difference_" + relPathString + ".html");
    std::ofstream outputFile(outputFileName);
    outputFile << htmlResult;
    outputFile.close();
  }
}

void renderArgoumlBenchmarkResultsToFiles(
    std::vector<DifferenceResult> differenceResults, Configuration config)
{
  ensureResultsDirectoryExists();

  std::string result { buildArgoumlBenchmarkOutput(differenceResults, config) };
  fs::path    outputFileName
      = fs::path("results") / "argouml_benchmark_results.txt";
  std::ofstream outputFile(outputFileName);
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

  // Ensure results directory exists
  ensureResultsDirectoryExists();

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
