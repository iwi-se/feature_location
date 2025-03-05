#include <iostream>
#include <vector>
#include <fstream>
#include "src/configuration.hpp"
#include "src/expression.hpp"
#include "src/evaluation.hpp"
#include "src/tree.hpp"
#include "render.hpp"
#include "argouml_benchmark_results.hpp"

void render_results_to_files(std::vector<DifferenceResult> difference_results, Configuration config)
{
    for (const auto &difference_result : difference_results)
    {
        std::string html_result = render_difference(difference_result, config);
        auto rel_path_string {difference_result.relative_path.string()};
        std::replace(rel_path_string.begin(), rel_path_string.end(), '/', '_');
        std::string output_file_name = "difference_" + rel_path_string + ".html";
        std::ofstream output_file(output_file_name);
        output_file << html_result;
        output_file.close();
    }
}

void render_argouml_benchmark_results_to_files(std::vector<DifferenceResult> difference_results, Configuration config)
{
    std::string result {build_argouml_benchmark_output(difference_results, config)};
    std::ofstream output_file("argouml_benchmark_results.txt");
    output_file << result;
    output_file.close();
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config.yaml>" << std::endl;
        return 1;
    }
    std::string config_file = argv[1];
    Configuration config(config_file);
    std::vector<ExpressionSystemName> expressions = config.getExpressionsToEvaluate();
    if (config.options.debug) {
        config.render();
    }

    for (const auto &expression : expressions)
    {
        auto difference_results = runExpression(expression, config);
        render_results_to_files(difference_results, config);
        render_argouml_benchmark_results_to_files(difference_results, config);
    }
    return 0;
}
