"""
@file feature_location_cli.py
@brief A command-line interface for feature isolation operations.

This CLI allows users to:
- Compute intersections of features across multiple source sets.
- Compute differences (highlighting changes).
- Show AST structures.
- Process configurations from YAML files.
"""

import re
import sys
import yaml
import os
from feature_location import read_and_preprocess, intersect_all_subtrees, difference, print_trees, read_and_preprocess_code
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
    result = [expr for expr in expressions if any(
        lbl in labels_to_run for lbl in expr["labels"])]
    return result


def gather_files(paths, options):
    """
    @brief Given a list of file or directory paths, returns all .java files found.
    @param paths A list of file or directory paths.
    @return A list of .java file paths.
    """
    language = options.get("language", "cpp")
    language_extensions = {
        "cpp": ["hpp", "h", "hxx", "h++", "hh", "h++", "cpp", "cxx", "c++", "cc", "c++", "cxx"],
        "java": ["java"],
    }
    files = []
    append = files.append
    for p in paths:
        if os.path.isdir(p):
            for root, dirs, files in os.walk(p):
                for file in files:
                    if file.endswith(tuple(language_extensions[language])):
                        append(os.path.join(root, file))
        else:
            if p.endswith(tuple(language_extensions[language])):
                append(p)
    return files


def process_file(file):
    """
    @brief Processes a YAML configuration file specifying an action and expressions.
    @param file The path to the YAML config file.
    """
    with open(file, "r") as f:
        config = yaml.safe_load(f)

    options = config.get("options", {})

    name_file_map = {m["name"]: m["file"]
                     for m in config["name-file-mappings"]}

    if config["action"] == "difference":
        exprs = find_expressions_to_run(config)
        for expr in exprs:
            left_side = map_systems_to_files(
                expr["left-side"], config, name_file_map)
            right_side = map_systems_to_files(
                expr["right-side"], config, name_file_map)

            left_files = [gather_files([left_side_file], options)
                          for left_side_file in left_side]
            right_files = [gather_files([right_side_file], options)
                           for right_side_file in right_side]

            os.makedirs("results", exist_ok=True)

            if all(len(files) == 1 for files in left_files + right_files):
                single_file_systems_left = [system[0] for system in left_files]
                single_file_systems_right = [system[0]
                                             for system in right_files]
                trees_before = [[read_and_preprocess(
                    file, options)] for file in single_file_systems_left]
                trees_after = [[read_and_preprocess(
                    file, options)] for file in single_file_systems_right]

                (resultTrees, source_ranges_subtraction) = difference(
                    trees_before, trees_after, options)
                source_ranges_intersection = [r.get_node(
                    r.root).data.source_positions for r in resultTrees]

                out_filename = os.path.join("results", "result.html")
                with open(out_filename, "w") as f:
                    f.write(render.render_feature_location(
                        single_file_systems_left, source_ranges_intersection,
                        single_file_systems_right, source_ranges_subtraction))
            else:
                output_file = os.path.join(
                    "results", "feature_location_" + str(expr["labels"][0]) + ".html")

                for i in range(len(left_files)):
                    process_difference_individual(
                        left_files[i], right_files[i], file_name=output_file, options=config.get("options", {}))
    else:
        print("Unknown action")


def read_args():
    """
    @brief Reads arguments after the first two arguments from sys.argv.
    @return A list of arguments starting from sys.argv[2].
    """
    return sys.argv[2:]


def combine_files_into_code(java_files):
    """
    @brief Combine multiple .java files into a single in-memory code string.
    @param java_files A list of .java filenames.
    @return A single string containing combined code from all files.
    """
    combined_lines = []
    for jf in java_files:
        combined_lines.append("// Contents from: {}\n".format(jf))
        with open(jf, "r", encoding='utf-8', errors='replace') as infile:
            combined_lines.append(infile.read())
            combined_lines.append("\n\n")
    return "".join(combined_lines)


def process_intersection(files):
    """
    @brief Processes the intersection of ASTs from given files/directories.
    @param files A list of files or directories.
    """
    all_files = gather_files(files)
    if not all_files:
        print("No .java files found for intersection.")
        return

    combined_code = combine_files_into_code(all_files)
    # Here we assume read_and_preprocess_code can process code strings directly
    # If not available, adapt feature_location accordingly.
    tree = read_and_preprocess_code(combined_code)
    # intersect_all_subtrees expects a list of lists of trees (if multiple sets are used)
    # Here we have just one set of trees from the combined code.
    trees = [[tree]]

    result = intersect_all_subtrees(trees)
    print_trees(result)

    os.makedirs("results", exist_ok=True)
    source_ranges = [res.get_node(
        res.root).data.source_positions for res in result]
    # We no longer have a temp file, but we can give a placeholder name
    with open(os.path.join("results", "feature_location.html"), "w") as f:
        f.write(render.render_feature_location(
            ["combined_memory.java"], source_ranges))


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
    @brief Process difference on a file-by-file basis without combining them into a temp file.
    @param before_java A list of .java files from the 'before' side.
    @param after_java A list of .java files from the 'after' side.
    @param file_name The output filename for the HTML feature location result.
    @param options Additional options passed to the difference operation.
    """
    os.makedirs("results", exist_ok=True)
    if len(before_java) == 2 and len(after_java) == 2 and all(os.path.isfile(f) for f in before_java+after_java):
        for i in range(2):
            b_file = before_java[i]
            a_file = after_java[i]

            treeBefore = [read_and_preprocess(b_file, options)]
            treeAfter = [read_and_preprocess(a_file, options)]

            (resultTrees, source_ranges_subtraction) = difference(
                [treeBefore], treeAfter, options)
            print_trees(resultTrees)
            source_ranges_intersection = [r.get_node(
                r.root).data.source_positions for r in resultTrees]

            base_name = os.path.splitext(os.path.basename(b_file))[
                0] + "_vs_" + os.path.splitext(os.path.basename(a_file))[0]
            out_filename = os.path.join("results", base_name + ".html")
            with open(out_filename, "w") as f:
                f.write(render.render_feature_location([b_file], source_ranges_intersection,
                                                       [a_file], source_ranges_subtraction))
        return

    before_map = {}
    for f in before_java:
        name = os.path.basename(f)
        before_map.setdefault(name, []).append(f)

    after_map = {}
    for f in after_java:
        name = os.path.basename(f)
        after_map.setdefault(name, []).append(f)

    for i, (filename, before_files) in enumerate(before_map.items()):
        if filename in after_map:
            after_files = after_map[filename]

            treeBefore = [read_and_preprocess(
                bf, options) for bf in before_files]
            treeAfter = [read_and_preprocess(af, options)
                         for af in after_files]

            (resultTrees, source_ranges_subtraction) = difference(
                [treeBefore], treeAfter, options)
            source_ranges_intersection = [r.get_node(
                r.root).data.source_positions for r in resultTrees]

            base_name, _ = os.path.splitext(filename)
            out_filename = os.path.join("results", base_name + ".html")
            with open(out_filename, "w") as f:
                f.write(render.render_feature_location(before_files, source_ranges_intersection,
                                                       after_files, source_ranges_subtraction))
        else:
            print(f"Can't find {filename} in after set.")

        print(f"Processed {i} of {len(before_map)} files: {filename}")


def process_difference(filesBeforeSeparator, filesAfterSeparator, file_name="feature_location.html", options={}):
    """
    @brief Processes the difference between two sets of files/directories on a file-by-file basis.
    @param filesBeforeSeparator A list of files/directories before the separator.
    @param filesAfterSeparator A list of files/directories after the separator.
    @param file_name The output filename for the HTML feature location result.
    @param options Additional options passed to the difference operation.
    """
    before_java = gather_files(filesBeforeSeparator)
    if not before_java:
        print("No .java files found in before-separator arguments.")
        return

    after_java = gather_files(filesAfterSeparator)
    if not after_java:
        print("No .java files found in after-separator arguments.")
        return

    before_trees = [read_and_preprocess(bf, options) for bf in before_java]
    after_trees = [read_and_preprocess(af, options) for af in after_java]

    before_result = intersect_all_subtrees([[t] for t in before_trees])
    before_intersection = [item[0] for item in before_result]

    after_result = intersect_all_subtrees([[t] for t in after_trees])
    after_intersection = [item[0] for item in after_result]
    (resultTrees, source_ranges_subtraction) = difference(
        [before_intersection], after_intersection, options)

    print_trees(resultTrees)
    os.makedirs("results", exist_ok=True)

    source_ranges_intersection = [r.get_node(
        r.root).data.source_positions for r in resultTrees]

    combined_before_name = [os.path.basename(f) for f in before_java]
    combined_after_name = [os.path.basename(f) for f in after_java]

    with open(os.path.join("results", file_name), "w") as f:
        f.write(render.render_feature_location(
            combined_before_name,
            source_ranges_intersection,
            combined_after_name,
            source_ranges_subtraction
        ))


def process_show_ast(files):
    """
    @brief Shows the AST of the given files/directories.
    @param files A list of files/directories.
    """
    all_java = gather_files(files)
    if not all_java:
        print("No .java files found.")
        return

    combined_code = combine_files_into_code(all_java)
    # use the code directly
    trees = [read_and_preprocess_code(
        combined_code, {"only_named_nodes": False})]
    print_trees(trees)

    os.makedirs("results", exist_ok=True)
    source_ranges = [t.get_node(t.root).data.source_positions for t in trees]
    with open(os.path.join("results", "feature_location.html"), "w") as f:
        f.write(render.render_feature_location(
            ["combined_memory.java"], source_ranges))


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
            left_numbers, right_numbers = parse_difference_expression(
                min_diff_str)
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
