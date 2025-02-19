#include <iostream>
#include <vector>
#include "src/configuration.hpp"
#include "src/expression.hpp"
#include "src/evaluation.hpp"
#include "src/tree.hpp"

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
        runExpression(expression, config);
    }
    return 0;
}