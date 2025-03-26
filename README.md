# Feature Location

This tool implements feature location techniques using set theory operations on the abstract syntax tree (AST) of the code. It allows you to identify the code elements that implement specific features in a software product line.

## Requirements

You need to have the following tools and libraries installed:

- A C++ compiler supporting C++20 (e.g., g++)
- make
- yaml-cpp
- nlohmann_json
- tree-sitter
- tree-sitter-cpp
- tree-sitter-java

### Installing Dependencies

#### Linux (Ubuntu/Debian)

```bash
# Install build essentials
sudo apt-get install -y build-essential

# Install yaml-cpp
sudo apt-get install -y libyaml-cpp-dev

# Install nlohmann_json
sudo apt-get install -y nlohmann-json3-dev

# Install tree-sitter and language parsers
# Clone and build tree-sitter
git clone https://github.com/tree-sitter/tree-sitter.git
cd tree-sitter
make
sudo make install

# Clone and build tree-sitter-cpp
git clone https://github.com/tree-sitter/tree-sitter-cpp.git
cd tree-sitter-cpp
make

# Clone and build tree-sitter-java
git clone https://github.com/tree-sitter/tree-sitter-java.git
cd tree-sitter-java
make
```

#### macOS (using Homebrew)

```bash
# Install dependencies
brew install yaml-cpp nlohmann-json tree-sitter
brew install tree-sitter-cpp tree-sitter-java
```

## Building the Project

The project uses a Makefile to manage the build process. It supports both debug and release builds:

```bash
# For debug build (default)
make

# OR explicitly specify debug build
make debug

# For release build (optimized)
make release
```

The executable will be generated in `obj/debug/` or `obj/release/` directory depending on the build type.

## Usage

The feature location tool requires a YAML configuration file that specifies the source code to analyze and the set operations to perform.

```bash
# For debug build
./obj/debug/feature_location <config.yaml>

# For release build
./obj/release/feature_location <config.yaml>
```

### Configuration File Format

The configuration file (YAML format) specifies:

- `action`: The action to perform (e.g., "difference")
- `name-path-mappings`: Maps system variant names to their respective file paths
- `options`: Configuration options including:
  - `minimum_trace_weight`: Minimum weight for including a trace, start with a value of 2 or 3 and adjust if necessary
  - `only_named_nodes`: Whether to only include named nodes. Named nodes is a treesitter concept. Non-named nodes are for examples, parentheses, semicolons, etc. Named nodes are semantically more important.
  - `language`: The programming language of the source code (e.g., "cpp" or "java")
  - `debug`: Enable debug output
  - `only_specific_nodes`: Limit analysis to specific node types (optional)
    - `node_types_file`: File for the specified language, provided by the Treesitter grammar, contains subtyping information
    - `node_types`: List of node types to include in the calculations. All subtypes of the specified node types will be considered as well.
- `expressions`: Set theory expressions to evaluate
  - `left-side`: List of system variants for the left side of the expression
  - `right-side`: List of system variants for the right side of the expression
  - `labels`: Labels for the results
- `run`: List of expressions to run (by their label)

Example configuration (example-config.yaml):

```yaml
action: difference
name-path-mappings:
  - name: S1
    paths: [example/r.hpp]
  - name: S2
    paths: [example/rl.hpp]
  # more mappings...
options:
  minimum_trace_weight: 2
  only_named_nodes: true
  language: cpp
  debug: false
  only_specific_nodes:
    node_types_file: "node_types_cpp.json"
    node_types: [
      "statement",
      "expression"
    ]
expressions:
  - left-side: [S2, S8]
    right-side: [S3, S5]
    labels:
      - logging
      - F
  - left-side: [S3, S8]
    right-side: [S1, S6]
    labels:
      - checked
      - F
  # more expressions...
run:
  - checked
```

Example usage:

```bash
./obj/debug/feature_location example-config.yaml
```

### Output

The tool generates output files in the `results` directory, which is created automatically if it doesn't exist:
- For each processed file, an HTML file named `difference_<relative_path>.html` is created in the results directory
- For Java projects, an additional benchmark results file `argouml_benchmark_results.txt` is generated in the results directory

## Project Structure

- `src/` - Contains all source code files including the main entry point (main.cpp)
- `obj/` - Contains build artifacts (object files and executables)
- `example/` - Contains example product line code
- `results/` - Contains generated output files (automatically created)
