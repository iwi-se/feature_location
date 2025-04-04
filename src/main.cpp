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
#include <chrono>
#include <iomanip>

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
                          Configuration                 config,
                          std::string                   expressionName)
{
  ensureResultsDirectoryExists();
  std::cout << "Rendering results..." << std::endl;

  for (const auto &differenceResult : differenceResults)
  {
    std::string htmlResult = renderDifference(differenceResult, config);
    auto        relPathString { differenceResult.relativePath.string() };
    std::replace(relPathString.begin(), relPathString.end(), '/', '_');
    fs::path outputFileName
        = fs::path("results")
          / ("difference_" + expressionName
             + (relPathString == "" ? "" : "_" + relPathString) + ".html");
    std::ofstream outputFile(outputFileName);
    outputFile << htmlResult;
    outputFile.close();
  }

  std::cout << "Results rendered." << std::endl;
}

void renderArgoumlBenchmarkResultsToFiles(
    std::vector<DifferenceResult> differenceResults, Configuration config)
{
  ensureResultsDirectoryExists();

  std::cout << "Creating ArgoUML benchmark results..." << std::endl;

  std::string result { buildArgoumlBenchmarkOutput(differenceResults, config) };
  fs::path    outputFileName
      = fs::path("results") / "argouml_benchmark_results.txt";
  std::ofstream outputFile(outputFileName);
  outputFile << result;
  outputFile.close();

  std::cout << "ArgoUML benchmark results created." << std::endl;
}

int main(int argc, char *argv[])
{
  auto startTime = std::chrono::high_resolution_clock::now();

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
    renderResultsToFiles(differenceResults, config, expression.labels[0]);
    if (config.options.language == "java")
    {
      renderArgoumlBenchmarkResultsToFiles(differenceResults, config);
    }
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration<double>(endTime - startTime);
  std::cout << "\nTotal execution time: " << std::fixed << std::setprecision(3) << duration.count() << " seconds" << std::endl;

  return 0;
}
