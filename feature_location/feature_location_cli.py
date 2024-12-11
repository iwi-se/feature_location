"""
@file feature_location_cli.py
@brief A command-line interface for feature location operations.

@note The intersection and show_ast functionalities remain similar to the original implementation. 
      The difference functionality has been adjusted to handle files individually based on matching names.
"""

import re
import sys
import yaml
import os
from feature_location import read_and_preprocess, intersect_all_subtrees, difference, print_trees
import render

def map_system_to_file(system, config):
    """
    @brief Maps a system name to a filename based on a given configuration.

    @param system The name of the system to map.
    @param config A dictionary-like configuration object that includes "name-file-mappings".
    @return The mapped filename or None if not found.
    """
    for mapping in config["name-file-mappings"]:
        if mapping["name"] == system:
            return mapping["file"]
    return None

def map_systems_to_files(systems, config):
    """
    @brief Maps a list of systems to their corresponding files.

    @param systems A list of system names.
    @param config Configuration object with "name-file-mappings".
    @return A list of filenames corresponding to the given systems.
    """
    return [map_system_to_file(system, config) for system in systems]

def find_expressions_to_run(config):
    """
    @brief Finds and returns a list of expressions to run based on the config.

    The config contains a "run" list and an "expressions" list. This function finds 
    all expressions whose labels match the items in "run".

    @param config A configuration dictionary containing "run" and "expressions".
    @return A list of expressions (dicts) that should be run.
    """
    expressions_to_run = []
    for label_to_run in config["run"]:
        for expr in config["expressions"]:
            if label_to_run in expr["labels"]:
                if expr not in expressions_to_run:
                    expressions_to_run.append(expr)
    return expressions_to_run

def gather_java_files(paths):
    """
    @brief Given a list of file or directory paths, returns all .java files found.

    If a path is a directory, it searches recursively. If it's a file and ends with .java, 
    it includes it directly. Mixing directories and files is allowed.

    @param paths A list of file or directory paths.
    @return A list of .java file paths found.
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

def process_file(file):
    """
    @brief Processes a YAML configuration file specifying an action and expressions.

    If the action is "difference", this function maps systems to files, gathers their .java files,
    and processes the difference similarly to how the CLI difference would work.

    @param file The path to the YAML config file.
    """
    config = {}
    with open(file, "r") as f:
        config = yaml.safe_load(f)

    if config["action"] == "difference":
        exprs = find_expressions_to_run(config)
        for expr in exprs:
            left_side = map_systems_to_files(expr["left-side"], config)
            right_side = map_systems_to_files(expr["right-side"], config)

            # Gather java files for left and right
            left_java_files = gather_java_files(left_side)
            right_java_files = gather_java_files(right_side)

            # Instead of combining into temp files, we now process them individually
            # for matching filenames.
            process_difference_individual(left_java_files, right_java_files, 
                                          "feature_location_" + str(expr["labels"][0]) + ".html", 
                                          config.get("options", {}))
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
    @brief Processes the intersection of the ASTs from given files/directories.

    Gathers .java files from the specified paths, combines them into a single 
    temporary file, and then finds the intersection of their feature locations.

    @param files A list of files or directories.
    """
    all_java_files = gather_java_files(files)
    if not all_java_files:
        print("No .java files found for intersection.")
        return

    # Combine into one temporary file as per original logic
    temp_file = combine_files_into_temp(all_java_files, "temp_intersection.java")

    trees = []
    trees.append([read_and_preprocess(temp_file)])

    print("Starting intersection", flush=True)

    result = intersect_all_subtrees(trees)
    print_trees(result)

    source_ranges = [res.get_node(res.root).data.source_positions for res in result]
    with open("feature_location.html", "w") as f:
        f.write(render.render_feature_location([temp_file], source_ranges))

def read_difference_args():
    """
    @brief Parses command-line arguments for the difference action.

    Everything before the separator "--" is considered 'before', and everything 
    after is considered 'after'.

    @return A tuple (filesBeforeSeparator, filesAfterSeparator) both lists of strings.
    """
    filesBeforeSeparator = []
    filesAfterSeparator = []
    afterSeparator = False
    separator = "--"
    for i in range(2, len(sys.argv)):
        if not afterSeparator and sys.argv[i] == separator:
            afterSeparator = True
        elif afterSeparator:
            filesAfterSeparator.append(sys.argv[i])
        else:
            filesBeforeSeparator.append(sys.argv[i])

    return filesBeforeSeparator, filesAfterSeparator

def process_difference_individual(before_java, after_java, file_name="feature_location.html", options={}):
    """
    @brief Process difference on a file-by-file basis without combining them.

    Given lists of Java files from the "before" side and the "after" side, 
    this function finds files with the same name and computes differences 
    between them individually.

    @param before_java A list of .java files from the 'before' side.
    @param after_java A list of .java files from the 'after' side.
    @param file_name The output filename for the HTML feature location result.
    @param options Additional options passed to the difference operation.
    """
    # Organize files by their base filename
    before_map = {}
    for f in before_java:
        name = os.path.basename(f)
        if name not in before_map:
            before_map[name] = []
        before_map[name].append(f)

    after_map = {}
    for f in after_java:
        name = os.path.basename(f)
        if name not in after_map:
            after_map[name] = []
        after_map[name].append(f)

    # For each file in before_map, we try to find it in after_map
    # If found, we run the difference. If not, print a message.
    for filename, before_files in before_map.items():
        if filename in after_map:
            # We'll consider all 'before_files' and 'after_files' that match this name.
            after_files = after_map[filename]

            # Note: If multiple files have the same name in multiple directories, 
            # we handle them all. 
            treesBefore = [read_and_preprocess(bf, options) for bf in before_files]
            treesAfter = [read_and_preprocess(af, options) for af in after_files]

            # difference() expects parameters as (list_of_lists, list_of_trees)
            # We'll treat treesBefore as one group and treesAfter as another.
            # intersection set: [treesBefore], subtraction set: treesAfter
            (resultTrees, source_ranges_subtraction) = difference([treesBefore], treesAfter, options)

            print_trees(resultTrees)
            source_ranges_intersection = [r.get_node(r.root).data.source_positions for r in resultTrees]
            with open(file_name.replace(".html", f"_{filename}.html"), "w") as f:
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

    # Process difference individually for each matching file
    process_difference_individual(before_java, after_java, file_name, options)

def process_show_ast(files):
    """
    @brief Shows the AST of the given files/directories.

    Gathers all .java files, combines them into a temporary file, and prints their AST.

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

def combine_files_into_temp(java_files, temp_filename):
    """
    @brief Combine multiple .java files into one temporary file.

    This function is used by intersection and show_ast operations, 
    but not by the modified difference operation.

    @param java_files A list of .java filenames.
    @param temp_filename The name of the temporary file to create.
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

    Extracts integer lists from the left and right parts of the expression.

    @param expression A string containing a difference expression.
    @return A tuple (left_numbers, right_numbers) both lists of integers.
    """
    left_part, right_part = expression.split(' \\ ')
    left_numbers = re.findall(r'\d+', left_part)
    right_numbers = re.findall(r'\d+', right_part)
    left_numbers = list(map(int, left_numbers))
    right_numbers = list(map(int, right_numbers))
    return left_numbers, right_numbers

def generate_yaml_from_isolation_result(file):
    """
    @brief Generates a YAML file from an isolation result file.

    Reads an isolation result file, extracts features and minimum differences, 
    and writes a YAML file containing differences as expressions.

    @param file The path to the isolation result file.
    """
    data = []
    with open(file, "r") as f:
        is_after_isolation_headline = False
        for line in f:
            if is_after_isolation_headline and "FEATURE ID" not in line:
                print(line)
                cols = line.split("\t")
                data.append({
                    "feature_id": cols[0],
                    "feature_name": cols[1],
                    "isolation_result": cols[2],
                    "number_min_differences": cols[3],
                    "min_differences": cols[4:]
                })
            if "=== ISOLATION RESULTS ===" in line:
                is_after_isolation_headline = True

    yaml_data = []
    for feature in data:
        for i, min_difference in enumerate(feature["min_differences"]):
            if min_difference.strip() == "":
                continue
            print(min_difference)
            left_numbers, right_numbers = parse_difference_expression(min_difference)
            yaml_data.append({
                "left-side": left_numbers,
                "right-side": right_numbers,
                "labels": [feature["feature_id"], feature["feature_name"], feature["feature_name"] + "_" + str(i)]
            })

    with open("isolation_results.yaml", "w") as f:
        yaml.dump(yaml_data, f)

if __name__ == "__main__":
    """
    @brief The main entry point of the script.

    Supported commands:
    - "intersection" followed by files/dirs
    - "difference" followed by files/dirs, then '--', then more files/dirs (modified to handle files individually)
    - "show_ast" followed by files/dirs
    - "file" followed by a YAML config file
    - "generate_yaml" followed by an isolation result file
    """
    if len(sys.argv) < 2:
        print("No arguments provided.")
        sys.exit(1)

    if sys.argv[1] == "intersection":
        process_intersection(read_args())

    elif sys.argv[1] == "difference":
        filesBeforeSeparator, filesAfterSeparator = read_difference_args()
        process_difference(filesBeforeSeparator, filesAfterSeparator)

    elif sys.argv[1] == "show_ast":
        process_show_ast(read_args())

    elif sys.argv[1] == "file":
        process_file(sys.argv[2])

    elif sys.argv[1] == "generate_yaml":
        generate_yaml_from_isolation_result(sys.argv[2])

    else:
        print("Unknown command")
