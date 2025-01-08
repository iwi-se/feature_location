# feature_location_cli.py
"""
@file feature_location_cli.py
@brief A command-line interface for feature isolation operations.

Usage:
  python feature_location_cli.py -h
  python feature_location_cli.py intersection [files_or_dirs...]
  python feature_location_cli.py difference [files_or_dirs...] -- [files_or_dirs...]
  python feature_location_cli.py show_ast [files_or_dirs...]
  python feature_location_cli.py generate_yaml <isolation_result_file>

Examples:
  python feature_location_cli.py intersection src/
  python feature_location_cli.py difference old/ -- new/
  python feature_location_cli.py show_ast MyClass.java
  python feature_location_cli.py generate_yaml isolation_result.csv
"""

import sys
import os
import yaml
import re
from calculation import (
    intersect_all_subtrees, difference, print_trees
)
from parsing import read_and_preprocess, read_and_preprocess_code
from html_renderer import HtmlRenderer

def print_help():
    """
    Prints an extensive explanation of how to use this script.
    """
    help_text = r"""
Usage: 
  python feature_location_cli.py [COMMAND] [ARGS...]

Commands:
  intersection [files_or_directories...]
    - Gathers all .java files from the given paths (recursively if directories),
      combines them into a single AST, and prints the intersected subtrees.
    - The results are also written to 'results/feature_location.html'.

  difference [files_or_directories...] -- [files_or_directories...]
    - Gathers .java files for two sets of inputs (before and after).
    - Computes the intersection of each set, then shows the difference between them.
    - Results are saved to 'results/feature_location.html'.

  show_ast [files_or_directories...]
    - Gathers all .java files, combines them into a single AST, then prints the AST.

  generate_yaml <isolation_result_file>
    - Reads the isolation result file (for example, something that was created with
      "=== ISOLATION RESULTS ===" inside) and generates 'isolation_results.yaml'"

Options:
  -h, --help
    - Prints this usage/help information.
"""
    print(help_text)

def dump_yaml(data, stream, indent=0, sort_keys=False):
    """
    A very basic YAML serializer that handles lists, dictionaries, and scalars.
    This does not support advanced YAML features (anchors, multi-line strings, etc.).
    """
    if isinstance(data, dict):
        # Optionally sort keys for consistent output
        keys = sorted(data.keys()) if sort_keys else data.keys()
        for key in keys:
            value = data[key]
            stream.write(' ' * indent + f"{key}:")
            if isinstance(value, (dict, list)):
                stream.write("\n")
                dump_yaml(value, stream, indent + 2, sort_keys)
            else:
                stream.write(f" {value}\n")
    elif isinstance(data, list):
        for item in data:
            stream.write(' ' * indent + "-")
            if isinstance(item, (dict, list)):
                stream.write("\n")
                dump_yaml(item, stream, indent + 2, sort_keys)
            else:
                stream.write(f" {item}\n")
    else:
        # scalar
        stream.write(' ' * indent + f"{data}\n")


def generate_yaml_from_isolation_result(file):
    """
    @brief Generates a YAML file from an isolation result file without using 'import yaml'.
    This version relies on our custom 'dump_yaml' function.
    """
    data = []
    with open(file, "r", encoding="utf-8", errors="replace") as f_in:
        is_after_isolation_headline = False
        for line in f_in:
            if is_after_isolation_headline and "FEATURE ID" not in line:
                cols = line.split("\t")
                if len(cols) < 5:
                    continue
                feature_id = cols[0]
                feature_name = cols[1]
                isolation_result = cols[2]
                number_min_differences = cols[3]
                min_differences = cols[4:]
                data.append({
                    "feature_id": feature_id,
                    "feature_name": feature_name,
                    "isolation_result": isolation_result,
                    "number_min_differences": number_min_differences,
                    "min_differences": min_differences
                })
            if "=== ISOLATION RESULTS ===" in line:
                is_after_isolation_headline = True

    yaml_data = []
    for feature in data:
        feature_id = feature["feature_id"]
        feature_name = feature["feature_name"]
        for i, min_difference in enumerate(feature["min_differences"]):
            min_diff_str = min_difference.strip()
            if not min_diff_str:
                continue
            left_numbers, right_numbers = parse_difference_expression(min_diff_str)
            yaml_data.append({
                "left-side": left_numbers,
                "right-side": right_numbers,
                "labels": [
                    feature_id,
                    feature_name,
                    f"{feature_name}_{i}"
                ]
            })

    # Write the YAML data out using our custom dump_yaml
    with open("isolation_results.yaml", "w", encoding="utf-8", errors="replace") as f_out:
        dump_yaml(yaml_data, f_out, sort_keys=False)


def gather_java_files(paths):
    """
    @brief Given a list of file or directory paths, returns all .java files found.
    """
    java_files = []
    for p in paths:
        if os.path.isdir(p):
            for root, dirs, files in os.walk(p):
                for file in files:
                    if file.endswith(".java"):
                        java_files.append(os.path.join(root, file))
        else:
            if p.endswith(".java"):
                java_files.append(p)
    return java_files

def combine_files_into_code(java_files):
    """
    @brief Combine multiple .java files into a single in-memory code string.
    """
    combined_lines = []
    for jf in java_files:
        combined_lines.append(f"// Contents from: {jf}\n")
        with open(jf, "r", encoding='utf-8', errors='replace') as infile:
            combined_lines.append(infile.read())
            combined_lines.append("\n\n")
    return "".join(combined_lines)

def process_intersection(files):
    """
    @brief Processes the intersection of ASTs from given files/directories.
    """
    all_java_files = gather_java_files(files)
    if not all_java_files:
        print("No .java files found for intersection.")
        return

    combined_code = combine_files_into_code(all_java_files)
    tree = read_and_preprocess_code(combined_code)

    trees = [[tree]]
    result = intersect_all_subtrees(trees)
    print_trees([r[0] for r in result])  # each r is (subtree, ratio)

    os.makedirs("results", exist_ok=True)
    # Build the list_of_trace_ranges from the intersections
    source_ranges = [r[0].get_node(r[0].root).data.source_positions for r in result]
    with open(os.path.join("results", "feature_location.html"), "w") as f:
        f.write(HtmlRenderer.render_feature_location(["combined_memory.java"], source_ranges))

def process_difference(files_before, files_after, file_name="feature_location.html", options={}):
    """
    @brief Processes the difference between two sets of files/directories on a file-by-file basis.
    """
    before_java = gather_java_files(files_before)
    after_java = gather_java_files(files_after)

    if not before_java or not after_java:
        print("No .java files found for difference.")
        return

    os.makedirs("results", exist_ok=True)

    before_trees = [read_and_preprocess(f, options) for f in before_java]
    after_trees = [read_and_preprocess(f, options) for f in after_java]

    before_result = intersect_all_subtrees([[t] for t in before_trees])
    after_result = intersect_all_subtrees([[t] for t in after_trees])

    before_intersection = [item[0] for item in before_result]
    after_intersection = [item[0] for item in after_result]

    (resultTrees, source_ranges_subtraction) = difference([before_intersection], after_intersection, options)
    print_trees(resultTrees)

    source_ranges_intersection = [r.get_node(r.root).data.source_positions for r in resultTrees]
    combined_before_name = [os.path.basename(f) for f in before_java]
    combined_after_name = [os.path.basename(f) for f in after_java]

    with open(os.path.join("results", file_name), "w") as f:
        f.write(HtmlRenderer.render_feature_location(
            combined_before_name,
            source_ranges_intersection,
            combined_after_name,
            source_ranges_subtraction
        ))

def process_show_ast(files):
    """
    @brief Shows the AST of the given files/directories.
    """
    all_java = gather_java_files(files)
    if not all_java:
        print("No .java files found.")
        return

    combined_code = combine_files_into_code(all_java)
    tree = read_and_preprocess_code(combined_code, {"only_named_nodes": False})
    print_trees([tree])

    results_dir = "results"
    os.makedirs(results_dir, exist_ok=True)

    source_ranges = [tree.get_node(tree.root).data.source_positions]

    code_contents = [combined_code]

def parse_difference_expression(expression: str):
    r"""
    @brief Parses a difference expression string of the form "X...Y \ A...B".
    For example: "10...20 \ 30...40" 
                 -> ([10, 20], [30, 40])
    """
    left_part, right_part = expression.split(' \\ ')
    left_numbers = list(map(int, re.findall(r'\d+', left_part)))
    right_numbers = list(map(int, re.findall(r'\d+', right_part)))
    return left_numbers, right_numbers

def main():
    if len(sys.argv) < 2:
        print_help()
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd in ("-h", "--help"):
        print_help()
        sys.exit(0)
    
    os.makedirs("results", exist_ok=True)

    if cmd == "intersection":
        process_intersection(sys.argv[2:])
    elif cmd == "difference":
        try:
            separator_index = sys.argv.index("--")
            before_files = sys.argv[2:separator_index]
            after_files = sys.argv[separator_index+1:]
        except ValueError:
            print("Usage: difference file1 [file2 ...] -- fileA [fileB ...]")
            sys.exit(1)
        process_difference(before_files, after_files)
    elif cmd == "show_ast":
        process_show_ast(sys.argv[2:])
    elif cmd == "file":
        process_file(sys.argv[2])
    elif cmd == "generate_yaml":
        if len(sys.argv) < 3:
            print("Usage: generate_yaml <isolation_result_file>")
            sys.exit(1)
        generate_yaml_from_isolation_result(sys.argv[2])
    else:
        print(f"Unknown command: {cmd}")
        print_help()

if __name__ == "__main__":
    main()
