#ifndef SET_OPERATIONS_HPP
#define SET_OPERATIONS_HPP

#include "tree.hpp"

std::pair<std::vector<SourcePosition>, std::vector<SourcePosition>> intersection(
    const std::shared_ptr<Node> &file1,
    const std::shared_ptr<Node> &file2);

#endif
