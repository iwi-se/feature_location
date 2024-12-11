from copy import deepcopy
import tree_sitter_java as tsjava
import tree_sitter_cpp as tscpp
import sys as sys
from tree_sitter import Language, Parser
from itertools import product
from treelib import Tree
import render

# Default configuration options
minimum_trace_size_default = 10
only_named_nodes_default = True

LANGUAGE = Language(tsjava.language())
parser = Parser(LANGUAGE)


class SourcePosition:
    """
    @brief Represents a source code position range in a file.
    @details Encapsulates file name, start and end points (line/column) of a code segment.
    """
    def __init__(self, file, start_point, end_point):
        """
        @brief Constructor for SourcePosition.
        @param file The filename where this source range is located.
        @param start_point The starting point (row, column) of the source segment.
        @param end_point The ending point (row, column) of the source segment.
        """
        self.file = file
        self.start_point = start_point
        self.end_point = end_point

    def render(self):
        """
        @brief Renders a human-readable string representation of the source position.
        @return A string showing filename and line/column ranges.
        """
        return (self.file + ":" +
                str(self.start_point.row + 1) + "/" + str(self.start_point.column + 1) +
                "-" + str(self.end_point.row + 1) + "/" + str(self.end_point.column + 1))

    def relative_position(self, other):
        """
        @brief Compares the relative position of this source segment to another.
        @param other Another SourcePosition object.
        @return An integer:
                -1 if self is entirely before other (non-overlapping),
                 0 if overlapping,
                 1 if self is entirely after other.
        """
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
        """
        @brief Checks equality of two SourcePosition objects.
        @param other Another SourcePosition object.
        @return True if both have the same file and start/end points, False otherwise.
        """
        return (self.file == other.file
                and self.start_point == other.start_point
                and self.end_point == other.end_point)

    def __hash__(self):
        """
        @brief Computes a hash for the SourcePosition, allowing it to be used in sets/dicts.
        @return The hash of the tuple (file, start_point, end_point).
        """
        return hash((self.file, self.start_point, self.end_point))

    def value_start_point(self):
        """
        @brief Computes a numeric value for the start point for sorting or comparison.
        @return An integer representing the start location (row*1000+column).
        """
        return (self.start_point.row * 1000 + self.start_point.column)


class NodeData:
    """
    @brief Holds data for a node in the AST tree.
    @details Includes node type, text, source positions, subtree hashes, etc.
    """
    def __init__(self, node_type, node_text, source_positions,
                 tree_positions, subtree_hash=None,
                 is_ts_leaf=False, indirect=False, subtree_size=None, is_named=False):
        """
        @brief Constructor for NodeData.
        @param node_type The type of the AST node.
        @param node_text The raw text associated with this node.
        @param source_positions A list of SourcePosition objects indicating the node's source code locations.
        @param tree_positions A list of lists representing the position of the node in the tree structure.
        @param subtree_hash A hash representing the subtree structure.
        @param is_ts_leaf A boolean indicating whether this is a leaf node in Tree-sitter terms.
        @param indirect A boolean flag (not currently used).
        @param subtree_size The number of descendants in this subtree.
        @param is_named A boolean indicating whether the node is named according to Tree-sitter.
        """
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
    """
    @brief Retrieves the root node of a given Tree.
    @param tree A treelib.Tree object.
    @return The root node object of the tree.
    """
    return tree.get_node(tree.root)


def hash_ts_node(ts_node):
    """
    @brief Computes a hash value for a Tree-sitter node.
    @param ts_node A Tree-sitter node.
    @return A hash value representing the node type and its children.
    """
    hash_self = hash(ts_node.type)
    hash_children = hash(str([hash_ts_node(child)
                             for child in ts_node.children]))
    if ts_node.child_count == 0:
        return hash((hash_self, hash(ts_node.text), hash_children))
    else:
        return hash((hash_self, hash_children))


def preprocess(treesitter_node, file, position, options={}):
    """
    @brief Converts a Tree-sitter node into a treelib.Tree structure with NodeData.
    @param treesitter_node The root Tree-sitter node from which to start.
    @param file The filename associated with this node.
    @param position A list representing the tree position (e.g., [0] for root).
    @param options Dictionary of options, including 'only_named_nodes'.
    @return A treelib.Tree with NodeData nodes.
    """
    tree = Tree()

    def traverse_and_build(parent_id, ts_node, position):
        node_data = NodeData(
            ts_node.type,
            ts_node.text,
            [SourcePosition(file, ts_node.start_point, ts_node.end_point)],
            [position],
            subtree_hash=hash_ts_node(ts_node),
            subtree_size=ts_node.descendant_count,
            is_named=ts_node.is_named
        )

        if ts_node.named_child_count == 0:
            node_data.is_ts_leaf = True

        node = tree.create_node(node_data.type, None, parent_id, node_data)

        for index, child in enumerate(ts_node.children):
            if child.is_named or not options.get("only_named_nodes", only_named_nodes_default):
                traverse_and_build(node.identifier, child, position + [index])

    traverse_and_build(None, treesitter_node, position)
    return tree


def read_and_preprocess(filename, options={}):
    """
    @brief Reads a C++ source file, parses it with Tree-sitter, and preprocesses it into a treelib.Tree.
    @param filename The C++ source file to parse.
    @param options Dictionary of parsing options.
    @return A treelib.Tree representing the AST of the file.
    """
    with open(filename, "rb") as f:
        content = f.read()
        tree = parser.parse(content)
        return preprocess(tree.root_node, filename, [0], options)


def positions_do_not_cross(positions, other_positions_list):
    """
    @brief Checks if the given positions do not cross with any set of other positions.
    @param positions A list of SourcePositions.
    @param other_positions_list A list of lists of SourcePositions to check against.
    @return True if there is no crossing, False otherwise.
    """
    if len(other_positions_list) == 0:
        return True
    for other_positions in other_positions_list:
        result = []
        for position, other_position in product(positions, other_positions):
            if position.file == other_position.file:
                result.append(position.relative_position(other_position))
        if not (len(result) == 0) and not (result[0] != 0 and all(x == result[0] for x in result)):
            return False
    return True


def remove_overlapping(trees):
    """
    @brief Removes overlapping trees from a list of trees.
    @details Ensures that no two trees share overlapping source positions.
    @param trees A list of treelib.Trees.
    @return A filtered list of trees without overlaps.
    """
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


def remove_overlapping_combinations(combinations):
    """
    @brief Removes overlapping subtree combinations.
    @param combinations A list of tuples (combination, ratio).
    @return A filtered list of combinations without overlapping root node positions.
    """
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
    """
    @brief Removes a subtree from the given tree, starting from a given node and going up.
    @param tree A treelib.Tree from which a subtree will be removed.
    @param node A node in the tree from which removal begins.
    @return A list of sibling subtrees and any removed ancestors.
    """
    result = []
    sibling_nodes = tree.siblings(node.identifier)
    sibling_trees = [tree.subtree(sibling_node.identifier)
                     for sibling_node in sibling_nodes]
    result.extend(sibling_trees)
    parent = tree.parent(node.identifier)
    if parent is not None:
        result.extend(remove_subtree(tree, parent))
    return result


def subtraction(leftSide, leftSideIntersected, treesToSubtract, options={}):
    """
    @brief Computes the positions to subtract from leftSide by considering the intersection with treesToSubtract.
    @param leftSide List of lists of treelib.Trees for the left side sets.
    @param leftSideIntersected List of treelib.Trees representing intersections in leftSide.
    @param treesToSubtract A list of treelib.Trees that will be used to subtract from the left side.
    @param options Dictionary of analysis options.
    @return A list of SourcePosition objects to subtract.
    """
    all_intersections = []
    for rightSideTree in treesToSubtract:
        all_intersections.extend(
            intersect_all_subtrees(deepcopy(leftSide) + [[deepcopy(rightSideTree)]], options))

    all_intersections.sort(key=lambda x: x[1], reverse=True)
    all_intersections = [x[0] for x in all_intersections]

    remove_overlapping(all_intersections)
    intersections_without_overlaps = all_intersections

    all_positions_to_subtract = []
    for intersection in intersections_without_overlaps:
        all_positions_to_subtract.extend(intersection.get_node(
            intersection.root).data.source_positions)

    return all_positions_to_subtract


def compute_combinations(trees, options={}):
    """
    @brief Computes combinations of matching subtrees that appear in all given trees.
    @param trees A list of treelib.Trees.
    @param options Dictionary of analysis options, such as minimum trace size.
    @return A list of tuples representing all subtree combinations found in all trees.
    """
    all_nodes_per_tree = []
    for tree in trees:
        all_nodes_per_tree.append(list(tree.filter_nodes(
            lambda x: x.tag != "common_root")))

    partition_by_subtree_hash = []
    for index, tree_ in enumerate(trees):
        current_dict = {}
        for node in all_nodes_per_tree[index]:
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
    """
    @brief Sorts trees by a decision ratio (placeholder function).
    @param trees A list of treelib.Trees.
    @return A list of treelib.Trees sorted by some computed ratio.
    """
    for tree in trees:
        calculate_decision_ratio(tree, tree, None)  # Placeholder usage
    sorted_nodes = sort_by_decision_ratio(nodes)  # 'nodes' undefined in snippet; placeholder
    sorted_trees = []
    for node in sorted_nodes:
        for tree in trees:
            if get_root_node(tree).data.source_positions == node[0].data.source_positions:
                sorted_trees.append(tree)
    return sorted_trees


def sort_by_size_to_remove_ratio(combinations):
    """
    @brief Sorts combinations based on a size-to-remove ratio.
    @param combinations A list of subtree combinations.
    @return The combinations sorted by size-to-remove ratio in descending order.
    """
    combination_with_ratio = []
    for combination in combinations:
        ratio = calculate_size_to_remove_ratio(combination, combinations)
        combination_with_ratio.append((combination, ratio))
    combination_with_ratio.sort(key=lambda comb: comb[1], reverse=True)
    return [x[0] for x in combination_with_ratio]


def calculate_size_to_remove_ratio(combination, combinations):
    """
    @brief Calculates a ratio representing how beneficial it is to remove a given subtree combination.
    @param combination A tuple/list of nodes forming a combination.
    @param combinations The entire set of combinations.
    @return A float ratio representing size-to-remove metric.
    """
    own_size = combination[0].data.subtree_size
    own_positions = [
        position for node in combination for position in node.data.source_positions]

    other_size_total = 0
    for other_combination in combinations:
        if other_combination is not combination:
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
    """
    @brief Calculates how close in depth the nodes of a combination are across trees.
    @param trees A list of treelib.Trees.
    @param combination A list of nodes, one from each tree.
    @return A depth proximity factor (float).
    """
    depths = []
    for index, node in enumerate(combination):
        depths.append(trees[index].depth(node.identifier))
    span = max(depths) - min(depths)
    depth_proximity = 1 / (span + 1)
    return depth_proximity


def get_ancestors(tree, node):
    """
    @brief Retrieves all ancestors of a given node.
    @param tree A treelib.Tree.
    @param node A node in the tree.
    @return A list of ancestor nodes.
    """
    ancestors = []
    while tree.parent(node.identifier) is not None:
        node = tree.parent(node.identifier)
        ancestors.append(node)
    return ancestors


def calculate_ancestor_similarity(trees, combination):
    """
    @brief Calculates how similar the ancestor chains of nodes in a combination are.
    @param trees A list of treelib.Trees.
    @param combination A list of nodes, one from each tree.
    @return A float indicating the fraction of matching ancestor chain length.
    """
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
    return similar_ancestors / (longest_ancestor_chain)


def calculate_sibling_similarity(trees, combination):
    """
    @brief Calculates similarity based on siblings of the nodes in a combination.
    @details Checks if siblings have similar subtree hashes and leaf texts.
    @param trees A list of treelib.Trees.
    @param combination A list of nodes, one from each tree.
    @return A float representing sibling similarity.
    """
    sibling_hashes_per_node = []
    for index, node in enumerate(combination):
        siblings = trees[index].siblings(node.identifier)
        sibling_hashes = [(sibling.data.subtree_hash, sibling.data.subtree_size, trees[index].leaves(node.identifier), sibling) for sibling in siblings]
        sibling_hashes_per_node.append(sibling_hashes)
    sibling_similarity = 0
    for sibling_hash in sibling_hashes_per_node[0]:
        sibling_in_all = True
        for i in range(1, len(sibling_hashes_per_node)):
            if sibling_hash not in sibling_hashes_per_node[i]:
                sibling_in_all = False
        if sibling_in_all:
            sibling_similarity += sibling_hash[1]
        else:
            # Check for partial leaf matches
            if sibling_hash[3].tag in [sib[3].tag for node_siblings in sibling_hashes_per_node[1:] for sib in node_siblings]:
                for leave in sibling_hash[2]:
                    leave_in_all = True
                    for i in range(1, len(sibling_hashes_per_node)):
                        for sib in sibling_hashes_per_node[i]:
                            if leave.data.text not in map(lambda x: x.data.text, sib[2]):
                                leave_in_all = False
                    if leave_in_all and leave.data.is_named:
                        sibling_similarity += 1

    def getParentSubtreeSize(t):
        tree, node = t
        parent = tree.parent(node.identifier)
        if parent is None or parent.data is None:
            return tree.size()
        return parent.data.subtree_size

    return sibling_similarity / (max(map(getParentSubtreeSize, zip(trees, combination))))


def calculate_environment_similarity(trees, combination):
    """
    @brief Calculates a combined environmental similarity measure including ancestor and sibling similarity.
    @param trees A list of treelib.Trees.
    @param combination A list of nodes, one from each tree.
    @return A float representing environment similarity.
    """
    ancestors_per_node = []
    for index, node in enumerate(combination):
        ancestors_per_node.append(get_ancestors(trees[index], node))

    direct_sibling_similarity = calculate_sibling_similarity(trees, combination)
    equal_ancestors = 0
    ancestor_sibling_similarities = 0

    ancestor_pairings = zip(*ancestors_per_node)

    for ancestor_pairing in ancestor_pairings:
        if all(ancestor.tag == ancestor_pairing[0].tag for ancestor in ancestor_pairing):
            equal_ancestors += 1
            ancestor_sibling_similarity = calculate_sibling_similarity(
                trees, ancestor_pairing)
            ancestor_sibling_similarities += ancestor_sibling_similarity
        else:
            break

    longest_ancestor_chain = max(map(len, ancestors_per_node))

    ancestor_similarity = equal_ancestors / longest_ancestor_chain
    ancestor_sibling_similarity = ancestor_sibling_similarities / (equal_ancestors + 1)
    environment_similarity = 0.2 * ancestor_similarity + \
        0.5 * direct_sibling_similarity + 0.3 * ancestor_sibling_similarity
    return environment_similarity


def calculate_decision_ratio(trees, combination, combinations):
    """
    @brief Calculates a decision ratio that factors in depth proximity, environment similarity, and subtree size.
    @param trees A list of treelib.Trees.
    @param combination A combination of nodes from each tree.
    @param combinations All combinations being considered (not currently used).
    @return A float representing the decision ratio.
    """
    depth_proximity = calculate_depth_proximity(trees, combination)
    size = combination[0].data.subtree_size
    environment_similarity = calculate_environment_similarity(trees, combination)
    decision_ratio = 0.2 * depth_proximity + \
        0.3 * environment_similarity + 0.5 * min((size / 100), 1)

    rendered = list(map(lambda x: str(
        list(map(lambda y: y.render(), x.data.source_positions))), combination))
    print(rendered, "DR:" + str(decision_ratio), "ES:" + str(environment_similarity), "DP:" + str(depth_proximity))

    return decision_ratio


def sort_by_decision_ratio(trees, combinations):
    """
    @brief Sorts combinations by their computed decision ratio.
    @param trees A list of treelib.Trees.
    @param combinations A list of node combinations.
    @return A list of tuples (combination, ratio) sorted by the decision ratio.
    """
    combination_with_ratio = []
    for combination in combinations:
        ratio = calculate_decision_ratio(trees, combination, combinations)
        combination_with_ratio.append((combination, ratio))
    combination_with_ratio.sort(key=lambda comb: (
        comb[1], comb[0][0].data.subtree_size, -sum(map(lambda x: x.data.source_positions[0].value_start_point(), comb[0]))), reverse=True)
    return combination_with_ratio


def intersect_all_subtrees(tree_groups, options={}):
    """
    @brief Finds all common subtrees among groups of trees.
    @param tree_groups A list of lists of treelib.Trees to intersect.
    @param options Dictionary of options for intersection.
    @return A list of tuples (subtree, ratio) representing matched subtrees found in all trees.
    """
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

    equal_combinations_with_ratio = sort_by_decision_ratio(
        trees, equal_combinations)

    equal_combinations_with_ratio = remove_overlapping_combinations(
        equal_combinations_with_ratio)

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
    """
    @brief Computes the difference between sets of trees by subtracting rightSide from leftSide.
    @param leftSide A list of lists of treelib.Trees.
    @param rightSide A list of treelib.Trees to subtract.
    @param options Dictionary of analysis options.
    @return A tuple (leftSideIntersected, positions_to_subtract).
    """
    leftSideIntersectedWithDecisionRatio = intersect_all_subtrees(
        deepcopy(leftSide), options)
    leftSideIntersected = [x[0] for x in leftSideIntersectedWithDecisionRatio]
    positions_to_subtract = subtraction(
        leftSide, leftSideIntersected, rightSide, options)
    return (leftSideIntersected, positions_to_subtract)


def print_tree(tree):
    """
    @brief Prints a tree to stdout in a human-readable format.
    @param tree A treelib.Tree to print.
    """
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
    """
    @brief Prints multiple trees, each separated by a line.
    @param trees A list of treelib.Trees.
    """
    print("\n")
    for tree in trees:
        print_tree(tree)
