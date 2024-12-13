"""
@file feature_isolation_cli.py
@brief A command-line interface for feature isolation operations.

This CLI allows users to:
- Compute intersections of features across multiple source sets.
- Compute differences (highlighting changes).
- Show AST structures.
- Process configurations from YAML files.

@note This file was originally feature_location_cli.py and has been renamed accordingly.
"""

import re
import sys
import yaml
import os
from feature_location import read_and_preprocess, intersect_all_subtrees, difference, print_trees
import render

def map_system_to_file(system, config, name_file_map):
    """
    @brief Maps a system name to a filename based on a given configuration.
    @param system The name of the system.
    @param config Configuration dictionary containing "name-file-mappings".
    @param name_file_map A prebuilt dictionary from name to file.
    @return The mapped filename or None if not found.
    """
    return name_file_map.get(system, None)

def map_systems_to_files(systems, config, name_file_map):
    """
    @brief Maps a list of systems to their corresponding files.
    @param systems A list of system names.
    @param config The config containing "name-file-mappings".
    @param name_file_map A dict from system name to file for fast lookups.
    @return A list of filenames corresponding to the systems.
    """
    return [map_system_to_file(system, config, name_file_map) for system in systems]

def find_expressions_to_run(config):
    """
    @brief Finds and returns a list of expressions to run based on config "run" and "expressions".
    @param config A dictionary with "run" and "expressions".
    @return A list of expressions (dicts) to be run.
    """
    labels_to_run = set(config["run"])
    expressions = config["expressions"]
    result = [expr for expr in expressions if any(lbl in labels_to_run for lbl in expr["labels"])]
    return result

def gather_java_files(paths):
    """
    @brief Given a list of file or directory paths, returns all .java files found.
    @param paths A list of file or directory paths.
    @return A list of .java file paths.
    """
    java_files = []
    append = java_files.append
    for p in paths:
        if os.path.isdir(p):
            for root, dirs, files in os.walk(p):
                for file in files:
                    if file.endswith(".java"):
                        append(os.path.join(root, file))
        else:
            if p.endswith(".java"):
                append(p)
    return java_files

def process_file(file):
    """
    @brief Processes a YAML configuration file specifying an action and expressions.
    @param file The path to the YAML config file.
    """
    with open(file, "r") as f:
        config = yaml.safe_load(f)

    name_file_map = {m["name"]: m["file"] for m in config["name-file-mappings"]}

    if config["action"] == "difference":
        exprs = find_expressions_to_run(config)
        for expr in exprs:
            left_side = map_systems_to_files(expr["left-side"], config, name_file_map)
            right_side = map_systems_to_files(expr["right-side"], config, name_file_map)

            left_java_files = gather_java_files(left_side)
            right_java_files = gather_java_files(right_side)

            os.makedirs("results", exist_ok=True)
            output_file = os.path.join("results", "feature_location_" + str(expr["labels"][0]) + ".html")
            process_difference_individual(left_java_files, right_java_files, file_name=output_file, options=config.get("options", {}))
    else:
        print("Unknown action")

def read_args():
    """
    @brief Reads arguments after the first two arguments from sys.argv.
    @return A list of arguments starting from sys.argv[2].
    """
    return sys.argv[2:]

def process_intersection(files):
    """
    @brief Processes the intersection of ASTs from given files/directories.
    @param files A list of files or directories.
    """
    all_java_files = gather_java_files(files)
    if not all_java_files:
        print("No .java files found for intersection.")
        return

    temp_file = combine_files_into_temp(all_java_files, "temp_intersection.java")

    trees = []
    trees.append([read_and_preprocess(temp_file)])

    result = intersect_all_subtrees(trees)
    print_trees(result)

    os.makedirs("results", exist_ok=True)
    source_ranges = [res.get_node(res.root).data.source_positions for res in result]
    with open(os.path.join("results", "feature_location.html"), "w") as f:
        f.write(render.render_feature_location([temp_file], source_ranges))

def read_difference_args():
    """
    @brief Parses command-line arguments for the difference action.
    Everything before the separator '--' is 'before', everything after is 'after'.
    @return A tuple (filesBeforeSeparator, filesAfterSeparator).
    """
    filesBeforeSeparator = []
    filesAfterSeparator = []
    afterSeparator = False
    separator = "--"
    argv = sys.argv
    length = len(argv)
    for i in range(2, length):
        arg = argv[i]
        if not afterSeparator and arg == separator:
            afterSeparator = True
        elif afterSeparator:
            filesAfterSeparator.append(arg)
        else:
            filesBeforeSeparator.append(arg)

    return filesBeforeSeparator, filesAfterSeparator

def process_difference_individual(before_java, after_java, file_name="feature_location.html", options={}):
    """
    @brief Process difference on a file-by-file basis without combining them.
    @param before_java A list of .java files from the 'before' side.
    @param after_java A list of .java files from the 'after' side.
    @param file_name The output filename for the HTML feature location result.
    @param options Additional options passed to the difference operation.
    """
    os.makedirs("results", exist_ok=True)

    before_map = {}
    for f in before_java:
        name = os.path.basename(f)
        before_map.setdefault(name, []).append(f)

    after_map = {}
    for f in after_java:
        name = os.path.basename(f)
        after_map.setdefault(name, []).append(f)

    for filename, before_files in before_map.items():
        if filename in after_map:
            after_files = after_map[filename]

            treesBefore = [read_and_preprocess(bf, options) for bf in before_files]
            treesAfter = [read_and_preprocess(af, options) for af in after_files]

            (resultTrees, source_ranges_subtraction) = difference([treesBefore], treesAfter, options)
            print_trees(resultTrees)
            source_ranges_intersection = [r.get_node(r.root).data.source_positions for r in resultTrees]

            base_name, _ = os.path.splitext(filename)
            out_filename = os.path.join("results", base_name + ".html")
            with open(out_filename, "w") as f:
                f.write(render.render_feature_location(before_files, source_ranges_intersection,
                                                       after_files, source_ranges_subtraction))
        else:
            print(f"Can't find {filename} in after set.")

def process_difference(filesBeforeSeparator, filesAfterSeparator, file_name="feature_location.html", options={}):
    """
    @brief Processes the difference between two sets of files/directories on a file-by-file basis.
    @param filesBeforeSeparator A list of files/directories before the separator.
    @param filesAfterSeparator A list of files/directories after the separator.
    @param file_name The output filename for the HTML feature location result.
    @param options Additional options passed to the difference operation.
    """
    before_java = gather_java_files(filesBeforeSeparator)
    if not before_java:
        print("No .java files found in before-separator arguments.")
        return

    after_java = gather_java_files(filesAfterSeparator)
    if not after_java:
        print("No .java files found in after-separator arguments.")
        return

    os.makedirs("results", exist_ok=True)
    process_difference_individual(before_java, after_java, file_name, options)

def process_show_ast(files):
    """
    @brief Shows the AST of the given files/directories.
    @param files A list of files/directories.
    """
    all_java = gather_java_files(files)
    if not all_java:
        print("No .java files found.")
        return
    temp_file = combine_files_into_temp(all_java, "temp_show_ast.java")

    trees = []
    trees.append(read_and_preprocess(temp_file, {"only_named_nodes": False}))
    print_trees(trees)

    os.makedirs("results", exist_ok=True)
    source_ranges = [t.get_node(t.root).data.source_positions for t in trees]
    with open(os.path.join("results", "feature_location.html"), "w") as f:
        f.write(render.render_feature_location([temp_file], source_ranges))

def combine_files_into_temp(java_files, temp_filename):
    """
    @brief Combine multiple .java files into one temporary file.
    @param java_files A list of .java filenames.
    @param temp_filename The name of the temporary file.
    @return The path to the created temporary file.
    """
    with open(temp_filename, "w", encoding='utf-8', errors='replace') as outfile:
        for jf in java_files:
            outfile.write("// Contents from: {}\n".format(jf))
            with open(jf, "r", encoding='utf-8', errors='replace') as infile:
                outfile.write(infile.read())
                outfile.write("\n\n")
    return temp_filename

def parse_difference_expression(expression: str):
    """
    @brief Parses a difference expression string of the form "X...Y \\ A...B".
    @param expression A string containing a difference expression.
    @return A tuple (left_numbers, right_numbers) both lists of integers.
    """
    left_part, right_part = expression.split(' \\ ')
    left_numbers = list(map(int, re.findall(r'\d+', left_part)))
    right_numbers = list(map(int, re.findall(r'\d+', right_part)))
    return left_numbers, right_numbers

def generate_yaml_from_isolation_result(file):
    """
    @brief Generates a YAML file from an isolation result file.
    @param file The path to the isolation result file.
    """
    data = []
    with open(file, "r") as f:
        is_after_isolation_headline = False
        for line in f:
            if is_after_isolation_headline and "FEATURE ID" not in line:
                cols = line.split("\t")
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
    yd_append = yaml_data.append
    for feature in data:
        feature_id = feature["feature_id"]
        feature_name = feature["feature_name"]
        for i, min_difference in enumerate(feature["min_differences"]):
            min_diff_str = min_difference.strip()
            if min_diff_str == "":
                continue
            left_numbers, right_numbers = parse_difference_expression(min_diff_str)
            yd_append({
                "left-side": left_numbers,
                "right-side": right_numbers,
                "labels": [feature_id, feature_name, feature_name + "_" + str(i)]
            })

    with open("isolation_results.yaml", "w") as f:
        yaml.dump(yaml_data, f)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("No arguments provided.")
        sys.exit(1)

    os.makedirs("results", exist_ok=True)

    cmd = sys.argv[1]
    if cmd == "intersection":
        process_intersection(read_args())
    elif cmd == "difference":
        filesBeforeSeparator, filesAfterSeparator = read_difference_args()
        process_difference(filesBeforeSeparator, filesAfterSeparator)
    elif cmd == "show_ast":
        process_show_ast(read_args())
    elif cmd == "file":
        process_file(sys.argv[2])
    elif cmd == "generate_yaml":
        generate_yaml_from_isolation_result(sys.argv[2])
    else:
        print("Unknown command")
