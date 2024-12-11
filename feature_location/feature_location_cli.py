import re
from feature_location import read_and_preprocess, intersect_all_subtrees, difference, print_trees
import render
import sys
import yaml

def map_system_to_file(system, config):
    """
    @brief Maps a system name to a filename using provided configuration.
    @param system The name of the system.
    @param config A dictionary containing name-file mappings.
    @return The corresponding filename or None if not found.
    """
    for mapping in config["name-file-mappings"]:
        if mapping["name"] == system:
            return mapping["file"]
    return None

def map_systems_to_files(systems, config):
    """
    @brief Maps multiple system names to filenames.
    @param systems A list of system names.
    @param config A dictionary with name-file mappings.
    @return A list of filenames corresponding to the given systems.
    """
    return [map_system_to_file(system, config) for system in systems]

def find_expressions_to_run(config):
    """
    @brief Finds which expressions should be run based on the 'run' list in the config.
    @param config A dictionary containing 'run' and 'expressions' keys.
    @return A list of expressions to run.
    """
    expressions_to_run = []
    for label_to_run in config["run"]:
        for expr in config["expressions"]:
            if label_to_run in expr["labels"]:
                if expr not in expressions_to_run:
                    expressions_to_run.append(expr)
    return expressions_to_run

def process_file(file):
    """
    @brief Processes a configuration file, running specified actions (like difference).
    @param file The configuration YAML file.
    """
    config = {}
    with open(file, "r") as f:
        config = yaml.safe_load(f)

    if config["action"] == "difference":
        exprs = find_expressions_to_run(config)
        for expr in exprs:
            left_side = map_systems_to_files(expr["left-side"], config)
            right_side = map_systems_to_files(expr["right-side"], config)
            process_difference(left_side, right_side, "feature_location_" + str(expr["labels"][0]) + ".html", config["options"])
    else:
        print("Unknown action")

def read_args():
    """
    @brief Reads arguments after the first command in sys.argv.
    @return A list of arguments provided.
    """
    return sys.argv[2:]

def process_intersection(files):
    """
    @brief Processes intersection of given files and prints the results.
    @param files A list of filenames.
    """
    trees = []
    for file in files:
        trees.append([read_and_preprocess(file)])

    print("Starting intersection", flush=True)

    result = intersect_all_subtrees(trees)
    print_trees(result)

    source_ranges = [result.get_node(
        result.root).data.source_positions for result in result]
    with open("feature_location.html", "w") as f:
        f.write(render.render_feature_location(sys.argv[2:], source_ranges))

def read_difference_args():
    """
    @brief Reads arguments for the 'difference' command, separating them by '--'.
    @return A tuple (filesBeforeSeparator, filesAfterSeparator).
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

def process_difference(filesBeforeSeparator, filesAfterSeparator, file_name="feature_location.html", options={}):
    """
    @brief Processes the difference between sets of files.
    @param filesBeforeSeparator Files on the left side of the difference.
    @param filesAfterSeparator Files on the right side of the difference.
    @param file_name Output HTML file name.
    @param options Dictionary of analysis options.
    """
    treesIntersection = [[read_and_preprocess(
        file, options)] for file in filesBeforeSeparator]
    treesSubtraction = [read_and_preprocess(
        file, options) for file in filesAfterSeparator]

    (treesIntersection, source_ranges_subtraction) = difference(treesIntersection, treesSubtraction, options)

    print_trees(treesIntersection)
    source_ranges_intersection = [result.get_node(
        result.root).data.source_positions for result in treesIntersection]
    with open(file_name, "w") as f:
        f.write(render.render_feature_location(
            filesBeforeSeparator, source_ranges_intersection, filesAfterSeparator, source_ranges_subtraction))

def process_show_ast(files):
    """
    @brief Processes and prints the AST of given files.
    @param files A list of filenames.
    """
    trees = []
    for file in files:
        trees.append(read_and_preprocess(file, {"only_named_nodes": False}))

    print_trees(trees)

def parse_difference_expression(expression: str):
    """
    @brief Parses a difference expression like 'X \\ Y' where X and Y contain numbers.
    @param expression A string containing a difference expression.
    @return Two lists (left_numbers, right_numbers) extracted from the expression.
    """
    left_part, right_part = expression.split(' \\ ')
    left_numbers = re.findall(r'\d+', left_part)
    right_numbers = re.findall(r'\d+', right_part)

    left_numbers = list(map(int, left_numbers))
    right_numbers = list(map(int, right_numbers))

    return left_numbers, right_numbers

def generate_yaml_from_isolation_result(file):
    """
    @brief Generates a YAML file from isolation results.
    @param file The input file containing isolation results.
    """
    data = []
    with open(file, "r") as f:
        is_after_isolation_headline = False
        for line in f:
            if is_after_isolation_headline and not "FEATURE ID" in line:
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
    @brief Entry point for the CLI tool. Handles commands like intersection, difference, show_ast, etc.
    """
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
