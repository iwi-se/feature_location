import tree_sitter_cpp as tscpp
import sys as sys
from tree_sitter import Language, Parser
from itertools import chain, product
from treelib import Tree
from math import prod

minimumFeatureSize = 1

CPP_LANGUAGE = Language(tscpp.language())

parser = Parser(CPP_LANGUAGE)


class SourcePosition:
    def __init__(self, file, start_point, end_point):
        self.file = file
        self.start_point = start_point
        self.end_point = end_point

    def render(self):
        return (self.file + ":" +
                str(self.start_point.row + 1) + "/" + str(self.start_point.column + 1) +
                "-" + str(self.end_point.row + 1) + "/" + str(self.end_point.column + 1))

    def relative_position(self, other):
        # -1: self is before other, non-overlapping
        # 0: self and other are overlapping in some way
        # 1: self is after other, non-overlapping
        if self.start_point.row > other.end_point.row:
            return -1
        elif self.end_point.row < other.start_point.row:
            return 1
        elif self.start_point.row == other.end_point.row:
            if self.start_point.column > other.end_point.column:
                return -1
        elif self.end_point.row == other.start_point.row:
            if self.end_point.column < other.start_point.column:
                return 1
        else:
            return 0

    def __eq__(self, other):
        return (self.file == other.file
                and self.start_point == other.start_point
                and self.end_point == other.end_point)

    def __hash__(self):
        return hash((self.file, self.start_point, self.end_point))


class NodeData:
    def __init__(self, node_type, node_text, source_positions: list[str],
                 tree_positions: list[list[int]], subtree_hash=None,
                 is_ts_leaf=False, indirect=False):
        self.type = node_type
        self.text = node_text
        self.source_positions = source_positions
        self.tree_positions = tree_positions
        self.is_ts_leaf = is_ts_leaf
        self.indirect = indirect
        self.subtree_hash = subtree_hash


def get_root_node(tree):
    return tree.get_node(tree.root)


def hash_ts_node(ts_node):
    hash_self = hash(ts_node.type)
    hash_children = hash(str([hash_ts_node(child)
                         for child in ts_node.children]))
    if ts_node.child_count == 0:
        return hash((hash_self, hash(ts_node.text), hash_children))
    else:
        return hash((hash_self, hash_children))


def preprocess(treesitter_node, file, position: list[int]):
    tree = Tree()

    def traverse_and_build(parent_id, ts_node, position):
        node_data = NodeData(ts_node.type, ts_node.text,
                             [SourcePosition(file, ts_node.start_point, ts_node.end_point)], [position], hash_ts_node(ts_node))

        if ts_node.named_child_count == 0:
            node_data.is_ts_leaf = True

        node = tree.create_node(node_data.type, None, parent_id, node_data)

        for index, child in enumerate(ts_node.children):
            # if child.is_named:
            traverse_and_build(node.identifier, child, position + [index])

    traverse_and_build(None, treesitter_node, position)
    return tree


def read_and_preprocess(filename):
    with open(filename, "rb") as f:
        content = f.read()
        tree = parser.parse(content)
        return preprocess(tree.root_node, filename, [0])


def positions_do_not_cross(positions, other_positions_list):
    if len(other_positions_list) == 0:
        return True
    for other_positions in other_positions_list:
        result = []
        for position, other_position in product(positions, other_positions):
            if position.file == other_position.file:
                result.append(position.relative_position(other_position))
        if not (result[0] != 0 and all(x == result[0] for x in result)):
            return False
    return True


def remove_overlapping(trees):
    result = []
    used_source_positions = set()
    used_root_node_source_positions = []
    for tree in trees:
        all_source_positions_in_tree = set()
        all_nodes = tree.all_nodes()
        root_node = tree.get_node(tree.root)
        for node in all_nodes:
            all_source_positions_in_tree.update(node.data.source_positions)
        if ((not (all_source_positions_in_tree & used_source_positions))
            and positions_do_not_cross(
                root_node.data.source_positions, used_root_node_source_positions)):
            result.append(tree)
            used_source_positions.update(all_source_positions_in_tree)
            used_root_node_source_positions.append(
                root_node.data.source_positions)
    return result


def subtraction(leftSide, treesToSubtract):
    all_intersections = []
    for treeToSubtract in treesToSubtract:
        all_intersections.extend(
            intersect_all_subtrees([leftSide, [treeToSubtract]]))

    all_intersections.sort(key=lambda x: x.size(), reverse=True)

    intersections_without_overlaps = remove_overlapping(all_intersections)

    all_positions_to_subtract = []  # enough to include root node positions
    for intersection in intersections_without_overlaps:
        all_positions_to_subtract.extend(intersection.get_node(
            intersection.root).data.source_positions)

    for tree in leftSide:
        for node in tree.all_nodes():
            # if set intersection not empty, remove
            if set(node.data.source_positions) & set(all_positions_to_subtract):
                try:
                    tree.remove_node(node.identifier)
                except:
                    pass
    return leftSide

def cart_product_of_equally_sized_subtrees(trees) -> list[list[Tree]]:
    all_nodes_per_tree = []
    for tree in trees:
        all_nodes_per_tree.append(list(tree.filter_nodes(
            lambda x: x.tag != "common_root")))

    partition_by_size = []
    for index, tree in enumerate(trees):
        nodes_per_size = {}
        for node in all_nodes_per_tree[index]:
            size = tree.subtree(node.identifier).size()
            if size not in nodes_per_size:
                nodes_per_size[size] = []
            nodes_per_size[size].append(node)
        partition_by_size.append(nodes_per_size)
    
    combinations = (x for x in [])
    all_sizes_collected = False
    i = 1
    while not all_sizes_collected:
        all_nodes_per_tree_size_i = []
        for sizeDict in partition_by_size:
            if i in sizeDict:
                all_nodes_per_tree_size_i.append(sizeDict[i])
            else:
                all_nodes_per_tree_size_i.append([])
        if all(len(x) > 0 for x in all_nodes_per_tree_size_i):
            cart_product = product(*all_nodes_per_tree_size_i)
            combinations = chain(combinations, cart_product)
        if min([tree.size() for tree in trees]) == i:
            all_sizes_collected = True
        i += 1

    return combinations

def intersect_all_subtrees(tree_groups):
    trees = []
    for group in tree_groups:
        common_tree = Tree()
        common_root = common_tree.create_node("common_root")
        for tree in group:
            common_tree.paste(common_root.identifier, tree, False)
        trees.append(common_tree)

    print("Intersecting " + str(len(trees)) + " trees", flush=True)
    all_combinations = cart_product_of_equally_sized_subtrees(trees)

    equal_combinations = []
    combination_count = prod([tree.size() for tree in trees])
    print("Comparing " + str(combination_count) + " node combinations", flush=True)
    for combination in all_combinations:
        if all(node.data.subtree_hash == combination[0].data.subtree_hash
               for node in combination):
            equal_combinations.append(combination)

    matched_subtrees = []
    for combination in equal_combinations:
        first_subtree = Tree(trees[0].subtree(
            combination[0].identifier), deep=True)
        all_nodes_first = first_subtree.all_nodes()
        for index in range(1, len(combination)):
            subtree = trees[index].subtree(combination[index].identifier)
            all_nodes = subtree.all_nodes()
            for node_first, node in zip(all_nodes_first, all_nodes):
                node_first.data.source_positions.extend(
                    node.data.source_positions)
                node_first.data.tree_positions.append(node.data.tree_positions)
        matched_subtrees.append(first_subtree)

    matched_subtrees.sort(key=lambda x: x.size(), reverse=True)

    return remove_overlapping(matched_subtrees)


def print_tree(tree):
    if tree.size() > 0:
        for node_id in tree.expand_tree(mode=Tree.DEPTH, sorting=False):
            print(4*" " * tree.depth(node_id), end="")
            node = tree.get_node(node_id)
            rendered = node.data.type + " " + \
                str(node.data.subtree_hash) + " " + \
                str(list(map(lambda x: x.render(), node.data.source_positions)))
            print(rendered)

        print("\n")
        print("------------------------------------------------")
        print("\n")


if __name__ == "__main__":

    if sys.argv[1] == "intersection":
        trees = []
        for i in range(2, len(sys.argv)):
            trees.append([read_and_preprocess(sys.argv[i])])

        print("Starting intersection", flush=True)
        result = intersect_all_subtrees(trees)
        for tree in result:
            print_tree(tree)

    elif sys.argv[1] == "difference":
        treesIntersection = []
        treesSubtraction = []
        afterSeparator = False
        separator = "--"
        for i in range(2, len(sys.argv)):
            if not afterSeparator and sys.argv[i] == separator:
                afterSeparator = True
            elif afterSeparator:
                treesSubtraction.append(read_and_preprocess(sys.argv[i]))
            else:
                treesIntersection.append([read_and_preprocess(sys.argv[i])])

        leftSide = intersect_all_subtrees(treesIntersection)

        results = subtraction(leftSide, treesSubtraction)

        for node in results:
            print_tree(node)

    elif sys.argv[1] == "show_ast":
        trees = []
        for i in range(2, len(sys.argv)):
            trees.append(read_and_preprocess(sys.argv[i]))

        for tree in trees:
            print_tree(tree)
