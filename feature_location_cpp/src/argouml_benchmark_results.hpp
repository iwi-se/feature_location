#ifndef ARGOUML_BENCHMARK_RESULTS_HPP
#define ARGOUML_BENCHMARK_RESULTS_HPP

#include <string>
#include <vector>
#include "configuration.hpp"
#include "set_operations.hpp"

std::string build_argouml_benchmark_output(
    std::vector<DifferenceResult> difference_results, Configuration config);

#endif
