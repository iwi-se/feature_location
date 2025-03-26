#ifndef ARGOUML_BENCHMARK_RESULTS_HPP
#define ARGOUML_BENCHMARK_RESULTS_HPP

#include "configuration.hpp"
#include "set_operations.hpp"
#include <string>
#include <vector>

std::string
    buildArgoumlBenchmarkOutput(std::vector<DifferenceResult> differenceResults,
                                Configuration                 config);

#endif
