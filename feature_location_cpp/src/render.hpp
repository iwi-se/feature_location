#ifndef RENDER_HPP
#define RENDER_HPP

#include "tree.hpp"
#include "configuration.hpp"

void render_file(std::filesystem::path file, std::vector<SourcePosition> source_positions, Configuration config);

#endif
