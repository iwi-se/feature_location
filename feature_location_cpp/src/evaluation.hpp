#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include <vector>
#include <string>
#include "configuration.hpp"
#include "expression.hpp"
#include "set_operations.hpp"

std::vector<DifferenceResult> runExpression(ExpressionSystemName expression, Configuration config);

ExpressionAllFiles getExpressionFiles(ExpressionSystemName expression, Configuration config);

std::vector<SingleFileExpression> buildFileBasedSubExpressions(ExpressionAllFiles expression, Configuration config);

//SingleFileExpressionResult evaluateExpression(SingleFileExpression expression, Configuration config);

#endif // EVALUATION_HPP