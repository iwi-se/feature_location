#include <iostream>
#include <vector>
#include "src/configuration.hpp"
#include "src/expression.hpp"
#include "src/evaluation.hpp"
#include "src/tree.hpp"
int main()
{
    Configuration config("../../example-config.yaml");
    std::vector<ExpressionSystemName> expressions = config.getExpressionsToEvaluate();
    config.render();
    for (const auto &expression : expressions)
    {
        runExpression(expression, config);
    }
    // for (const auto &expression : expressions)
    // {
    //     EvaluationResult result = evaluateExpression(expression, config);
    //     renderResult(result);
    // }
    return 0;
}