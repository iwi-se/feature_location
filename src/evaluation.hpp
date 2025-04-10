#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include "configuration.hpp"
#include "expression.hpp"
#include "set_operations.hpp"
#include <string>
#include <vector>

std::vector<DifferenceResult> runExpression(ExpressionSystemName expression,
                                            const Configuration        &config);

ExpressionAllFiles getExpressionFiles(ExpressionSystemName expression,
                                      const Configuration        &config);

std::vector<SingleFileExpression>
    buildFileBasedSubExpressions(ExpressionAllFiles expression);

#endif // EVALUATION_HPP
