import tree_sitter_cpp as tscpp
import sys as sys
from tree_sitter import Language, Parser
from itertools import product
from treelib import Tree
import render

minimum_trace_size_default = 1

only_named_nodes_default = True

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
        if self.end_point.row < other.start_point.row:
            return 1
        if self.start_point.row == other.end_point.row:
            if self.start_point.column >= other.end_point.column:
                return -1
        if self.end_point.row == other.start_point.row:
            if self.end_point.column <= other.start_point.column:
                return 1
        return 0

    def __eq__(self, other):
        return (self.file == other.file
                and self.start_point == other.start_point
                and self.end_point == other.end_point)

    def __hash__(self):
        return hash((self.file, self.start_point, self.end_point))

    def value_start_point(self):
        return (self.start_point.row * 1000 + self.start_point.column)


class NodeData:
    def __init__(self, node_type, node_text, source_positions: list[str],
                 tree_positions: list[list[int]], subtree_hash=None,
                 is_ts_leaf=False, indirect=False, subtree_size=None, is_named=False):
        self.type = node_type
        self.text = node_text
        self.source_positions = source_positions
        self.tree_positions = tree_positions
        self.is_ts_leaf = is_ts_leaf
        self.indirect = indirect
        self.subtree_hash = subtree_hash
        self.subtree_size = subtree_size
        self.is_named = is_named


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


def preprocess(treesitter_node, file, position: list[int], options={}):
    tree = Tree()

    def traverse_and_build(parent_id, ts_node, position):
        node_data = NodeData(ts_node.type, ts_node.text,
                             [SourcePosition(file, ts_node.start_point, ts_node.end_point)], [position], hash_ts_node(ts_node), subtree_size=ts_node.descendant_count, is_named=ts_node.is_named)

        if ts_node.named_child_count == 0:
            node_data.is_ts_leaf = True

        node = tree.create_node(node_data.type, None, parent_id, node_data)

        for index, child in enumerate(ts_node.children):
            if child.is_named or not options.get("only_named_nodes", only_named_nodes_default):
                traverse_and_build(node.identifier, child, position + [index])

    traverse_and_build(None, treesitter_node, position)
    return tree


def read_and_preprocess(filename, options={}):
    with open(filename, "rb") as f:
        content = f.read()
        tree = parser.parse(content)
        return preprocess(tree.root_node, filename, [0], options)


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


# Extra variant for combinations because perfomance
def remove_overlapping_combinations(combinations):
    result = []
    used_root_node_source_positions = []
    for (combination, ratio) in combinations:
        all_root_node_source_positions = [
            position for node in combination for position in node.data.source_positions]
        if positions_do_not_cross(
                all_root_node_source_positions, used_root_node_source_positions):
            result.append((combination, ratio))
            used_root_node_source_positions.append(
                all_root_node_source_positions)
    return result


def remove_subtree(tree, node):
    result = []
    sibling_nodes = tree.siblings(node.identifier)
    sibling_trees = [tree.subtree(sibling_node.identifier)
                     for sibling_node in sibling_nodes]
    result.extend(sibling_trees)
    parent = tree.parent(node.identifier)
    if parent is not None:
        result.extend(remove_subtree(tree, parent))
    return result


def subtraction(leftSide, treesToSubtract, options={}):
    # Intersect treesToSubtract first
    # intersection_trees_to_subtract = intersect_all_subtrees(
    #     [[tree] for tree in treesToSubtract], options)

    all_intersections = []
    # all_intersections.extend(intersect_all_subtrees(
    #     [leftSide, intersection_trees_to_subtract], options))

    for treeToSubtract in treesToSubtract:
        all_intersections.extend(
            intersect_all_subtrees([leftSide, [treeToSubtract]], options))

    all_intersections.sort(key=lambda x: x[1], reverse=True)

    all_intersections = [x[0] for x in all_intersections]

    intersections_without_overlaps = remove_overlapping(all_intersections)

    all_positions_to_subtract = []  # enough to include root node positions
    for intersection in intersections_without_overlaps:
        all_positions_to_subtract.extend(intersection.get_node(
            intersection.root).data.source_positions)

    result = []
    for tree in leftSide:
        tree_result = [tree]
        for node in tree.all_nodes():
            # if set intersection not empty, remove
            if set(node.data.source_positions) & set(all_positions_to_subtract):
                for res_tree in tree_result:
                    if res_tree.contains(node.identifier):
                        tree_result.remove(res_tree)
                        tree_result.extend(remove_subtree(res_tree, node))
                        break
        result.extend(tree_result)
    return result


def compute_combinations(trees, options={}) -> list[list[Tree]]:
    all_nodes_per_tree = []
    for tree in trees:
        all_nodes_per_tree.append(list(tree.filter_nodes(
            lambda x: x.tag != "common_root")))

    partition_by_subtree_hash = []
    for index, tree in enumerate(trees):
        current_dict = {}
        for node in all_nodes_per_tree[index]:
            # if node.data.type in ["{", "}", "(", ")", "[", "]", ";", ",", "#include"] or node.data.subtree_size < options.get("minimum_trace_size", minimum_trace_size_default):
            if node.data.subtree_size < options.get("minimum_trace_size", minimum_trace_size_default):
                continue
            hash_ = node.data.subtree_hash
            if hash_ not in current_dict:
                current_dict[hash_] = []
            current_dict[hash_].append(node)
        partition_by_subtree_hash.append(current_dict)

    combinations = []
    for hash_, nodes in partition_by_subtree_hash[0].items():
        nodes_with_same_hash = [nodes]
        for index in range(1, len(partition_by_subtree_hash)):
            if hash_ in partition_by_subtree_hash[index]:
                nodes_with_same_hash.append(
                    partition_by_subtree_hash[index][hash_])
            else:
                break
        if len(nodes_with_same_hash) == len(partition_by_subtree_hash):
            combinations.extend(list(product(*nodes_with_same_hash)))

    return combinations


def sort_by_size_to_remove_ratio_tree(trees):

    for tree in trees:
        calculate_decision_ratio(tree, tree, None)
    sorted_nodes = sort_by_decision_ratio(nodes)
    sorted_trees = []
    for node in sorted_nodes:
        for tree in trees:
            if get_root_node(tree).data.source_positions == node[0].data.source_positions:
                sorted_trees.append(tree)
    return sorted_trees


def sort_by_size_to_remove_ratio(combinations):
    combination_with_ratio = []
    for combination in combinations:
        ratio = calculate_size_to_remove_ratio(combination, combinations)
        combination_with_ratio.append((combination, ratio))
    combination_with_ratio.sort(key=lambda comb: comb[1], reverse=True)
    return [x[0] for x in combination_with_ratio]


def calculate_size_to_remove_ratio(combination, combinations):
    own_size = combination[0].data.subtree_size
    own_positions = [
        position for node in combination for position in node.data.source_positions]

    # TODO: do not include other combination that are completely contained in this one
    other_size_total = 0
    for other_combination in combinations:
        if not other_combination is combination:
            other_positions = [
                position for node in other_combination for position in node.data.source_positions]
            if not positions_do_not_cross(own_positions, [other_positions]):
                other_size = other_combination[0].data.subtree_size
                other_size_total += other_size
    size_to_remove = (10 * own_size) / (other_size_total + 1)
    normalized_size_to_remove = 1 - (size_to_remove / own_size)
    print(normalized_size_to_remove, size_to_remove, own_size, other_size_total)
    return normalized_size_to_remove


def calculate_depth_proximity(trees, combination):
    depths = []
    for index, node in enumerate(combination):
        depths.append(trees[index].depth(node.identifier))
    span = max(depths) - min(depths)
    depth_proximity = 1 / (span + 1)
    return depth_proximity


def get_ancestors(tree, node):
    ancestors = []
    while tree.parent(node.identifier) is not None:
        node = tree.parent(node.identifier)
        ancestors.append(node)
    return ancestors


def calculate_ancestor_similarity(trees, combination):
    ancestors_per_node = []
    for index, node in enumerate(combination):
        ancestors_per_node.append(get_ancestors(trees[index], node))
    similar_ancestors = 0
    for ancestor_tuple in zip(*ancestors_per_node):
        all_ancestors_same = True
        for ancestor in ancestor_tuple:
            if ancestor.tag != ancestor_tuple[0].tag:
                all_ancestors_same = False
        if all_ancestors_same:
            similar_ancestors += 1
        else:
            break
    longest_ancestor_chain = max(map(len, ancestors_per_node))
    # print(similar_ancestors / longest_ancestor_chain, similar_ancestors, longest_ancestor_chain)
    return similar_ancestors / (longest_ancestor_chain)


def calculate_sibling_similarity(trees, combination):
    sibling_hashes_per_node = []
    for index, node in enumerate(combination):
        siblings = trees[index].siblings(node.identifier)
        sibling_hashes = [sibling.data.subtree_hash for sibling in siblings]
        sibling_hashes_per_node.append(sibling_hashes)
    sibling_similarity = 0
    for sibling_hash in sibling_hashes_per_node[0]:
        for i in range(1, len(sibling_hashes_per_node)):
            if sibling_hash in sibling_hashes_per_node[i]:
                sibling_similarity += 1
    return sibling_similarity / (max(map(len, sibling_hashes_per_node)) + 1)


def calculate_decision_ratio(trees, combination, combinations):
    #size_to_remove_ratio = calculate_size_to_remove_ratio(
    #    combination, combinations)
    depth_proximity = calculate_depth_proximity(trees, combination)
    ancestor_similarity = calculate_ancestor_similarity(trees, combination)
    sibling_similarity = calculate_sibling_similarity(trees, combination)
    decision_ratio = 0.25 * ancestor_similarity + 0.65 * sibling_similarity + 0.1 * depth_proximity
    rendered = list(map(lambda x: str(list(map(lambda y: y.render(), x.data.source_positions))), combination))
    print(rendered, "DR:" + str(decision_ratio), "SR:" + str("Currently omitted"), "AS:" + str(ancestor_similarity), "SS:" + str(sibling_similarity), "DP:" + str(depth_proximity))
    return decision_ratio


def sort_by_decision_ratio(trees, combinations):
    combination_with_ratio = []
    for combination in combinations:
        ratio = calculate_decision_ratio(trees, combination, combinations)
        combination_with_ratio.append((combination, ratio))
    combination_with_ratio.sort(key=lambda comb: (
        comb[1], comb[0][0].data.subtree_size, -sum(map(lambda x: x.data.source_positions[0].value_start_point(), comb[0]))), reverse=True)
    return combination_with_ratio


def intersect_all_subtrees(tree_groups, options={}):
    if not tree_groups:
        return []
    trees = []
    for group in tree_groups:
        common_tree = Tree()
        common_root = common_tree.create_node("common_root")
        for tree in group:
            common_tree.paste(common_root.identifier, tree, False)
        trees.append(common_tree)

    print("Intersecting " + str(len(trees)) + " trees", flush=True)

    equal_combinations = compute_combinations(trees, options)

    print("Found " + str(len(equal_combinations)) +
          " subtrees that occur in all trees", flush=True)

    equal_combinations_with_ratio = sort_by_decision_ratio(trees, equal_combinations)

    equal_combinations_with_ratio = remove_overlapping_combinations(equal_combinations_with_ratio)

    matched_subtrees = []

    for (combination, ratio) in equal_combinations_with_ratio:
        first_subtree = Tree(trees[0].subtree(
            combination[0].identifier))
        all_nodes_first = first_subtree.all_nodes()
        for index in range(1, len(combination)):
            subtree = trees[index].subtree(combination[index].identifier)
            all_nodes = subtree.all_nodes()
            for node_first, node in zip(all_nodes_first, all_nodes):
                for position in node.data.source_positions:
                    if position not in node_first.data.source_positions:
                        node_first.data.source_positions.append(position)
                for position in node.data.tree_positions:
                    if position not in node_first.data.tree_positions:
                        node_first.data.tree_positions.append(position)
        matched_subtrees.append((first_subtree, ratio))

    return matched_subtrees


def difference(leftSide, rightSide, options={}):
    leftSideIntersected = intersect_all_subtrees(leftSide, options)
    return subtraction([x[0] for x in leftSideIntersected], rightSide, options)


def print_tree(tree):
    if tree.size() > 0:
        for node_id in tree.expand_tree(mode=Tree.DEPTH, sorting=False):
            print(4*" " * tree.depth(node_id), end="")
            node = tree.get_node(node_id)
            rendered = node.data.type + " " + \
                (str(node.data.text) if node.data.is_ts_leaf else "") + \
                " " + \
                str(node.data.subtree_hash) + " " + \
                str(list(map(lambda x: x.render(), node.data.source_positions)))
            print(rendered)

        print("\n")
        print("------------------------------------------------")
        print("\n")


def print_trees(trees):
    print("\n")
    for tree in trees:
        print_tree(tree)
