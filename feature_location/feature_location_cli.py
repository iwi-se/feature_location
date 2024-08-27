from feature_location import read_and_preprocess, intersect_all_subtrees, difference, print_trees
import render
import sys
import yaml

def map_system_to_file(system, config):
    for mapping in config["name-file-mappings"]:
        if mapping["name"] == system:
            return mapping["file"]
    return None

def map_systems_to_files(systems, config):
    return [map_system_to_file(system, config) for system in systems]

def find_expressions_to_run(config):
    expressions_to_run = []
    for label_to_run in config["run"]:
        for expr in config["expressions"]:
            if label_to_run in expr["labels"]:
                if expr not in expressions_to_run:
                    expressions_to_run.append(expr)
    return expressions_to_run

def process_file(file):
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
    return sys.argv[2:]

def process_intersection(files):
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
    treesIntersection = [[read_and_preprocess(
        file, options)] for file in filesBeforeSeparator]
    treesSubtraction = [read_and_preprocess(
        file, options) for file in filesAfterSeparator]

    results = difference(treesIntersection, treesSubtraction, options)

    print_trees(results)
    source_ranges = [result.get_node(
        result.root).data.source_positions for result in results]
    with open(file_name, "w") as f:
        f.write(render.render_feature_location(
            filesBeforeSeparator, source_ranges))


def process_show_ast(files):
    trees = []
    for file in files:
        trees.append(read_and_preprocess(file))

    print_trees(trees)


if __name__ == "__main__":

    if sys.argv[1] == "intersection":
        process_intersection(read_args())

    elif sys.argv[1] == "difference":
        filesBeforeSeparator, filesAfterSeparator = read_difference_args()
        process_difference(filesBeforeSeparator, filesAfterSeparator)

    elif sys.argv[1] == "show_ast":
        process_show_ast(read_args())

    elif sys.argv[1] == "file":
        process_file(sys.argv[2])
