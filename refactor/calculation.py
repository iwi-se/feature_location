# calculation.py
"""
@file calculation.py
@brief Contains functions for computing intersections, differences, and other AST analyses.
"""

from parsing import read_and_preprocess, read_and_preprocess_code, hash_ts_node
from position import SourcePosition
from tree import Tree, deepcopy

minimum_trace_size_default = 100

def product(*iterables):
    """
    Custom implementation of product, which generates the Cartesian product
    of the provided iterables.
    """
    pools = [list(pool) for pool in iterables]
    if not pools:
        yield ()
        return
    indexes = [0] * len(pools)
    yield tuple(pool[0] for pool in pools)
    while True:
        for i in reversed(range(len(pools))):
            indexes[i] += 1
            if indexes[i] < len(pools[i]):
                yield tuple(pools[j][indexes[j]] for j in range(len(pools)))
                break
            indexes[i] = 0
        else:
            return

def positions_do_not_cross(positions, other_positions_list):
    """
    @brief Checks if the given positions do not overlap with any sets of other positions.
    @param positions A list of SourcePosition objects.
    @param other_positions_list A list of lists of SourcePosition objects.
    @return True if no crossing is found, False otherwise.
    """
    if not other_positions_list:
        return True

    all_other_positions = []
    for opl in other_positions_list:
        all_other_positions.extend(opl)

    def start_key(p):
        return (p.start_point.row, p.start_point.column)

    positions_sorted = sorted(positions, key=start_key)
    others_sorted = sorted(all_other_positions, key=start_key)

    i, j = 0, 0
    while i < len(positions_sorted) and j < len(others_sorted):
        p = positions_sorted[i]
        o = others_sorted[j]
        # If p is entirely before o
        if (p.end_point.row < o.start_point.row or
           (p.end_point.row == o.start_point.row and p.end_point.column <= o.start_point.column)):
            i += 1
        # If p is entirely after o
        elif (p.start_point.row > o.end_point.row or
             (p.start_point.row == o.end_point.row and p.start_point.column >= o.end_point.column)):
            j += 1
        else:
            return False

    return True

def remove_overlapping(trees):
    """
    @brief Removes overlapping trees from a list of trees.
    @param trees A list of Trees.
    @return A filtered list without overlaps.
    """
    result = []
    used_source_positions = set()
    used_root_node_source_positions = []
    for tree in trees:
        all_nodes = tree.all_nodes()
        all_source_positions_in_tree = {pos for node in all_nodes for pos in node.data.source_positions}

        root_node = tree.get_node(tree.root)
        root_positions = root_node.data.source_positions

        if not (all_source_positions_in_tree & used_source_positions) \
           and positions_do_not_cross(root_positions, used_root_node_source_positions):
            result.append(tree)
            used_source_positions.update(all_source_positions_in_tree)
            used_root_node_source_positions.append(root_positions)
    return result

def remove_overlapping_combinations(combinations):
    """
    @brief Removes overlapping subtree combinations.
    @param combinations A list of tuples (combination, ratio).
    @return A filtered list of combinations.
    """
    result = []
    used_root_node_source_positions = []
    for (combination, ratio) in combinations:
        all_root_node_source_positions = []
        for node in combination:
            all_root_node_source_positions.extend(node.data.source_positions)
        if positions_do_not_cross(all_root_node_source_positions, used_root_node_source_positions):
            result.append((combination, ratio))
            used_root_node_source_positions.append(all_root_node_source_positions)
    return result

def compute_combinations(trees, options={}):
    """
    @brief Computes combinations of matching subtrees that appear in all given trees.
    @param trees A list of Trees.
    @param options Dictionary of options.
    @return A list of subtree combinations.
    """
    min_trace_size = options.get("minimum_trace_size", minimum_trace_size_default)
    all_nodes_per_tree = [
        [x for x in tree.filter_nodes(lambda x: x.tag != "common_root") if x.data.subtree_size >= min_trace_size]
        for tree in trees
    ]

    partition_by_subtree_hash = []
    for index, tree_ in enumerate(trees):
        current_dict = {}
        for node in all_nodes_per_tree[index]:
            hash_ = node.data.subtree_hash
            current_dict.setdefault(hash_, []).append(node)
        partition_by_subtree_hash.append(current_dict)

    combinations = []
    base_dict = partition_by_subtree_hash[0]
    length = len(partition_by_subtree_hash)
    for hash_, nodes in base_dict.items():
        nodes_with_same_hash = [nodes]
        for idx in range(1, length):
            d = partition_by_subtree_hash[idx]
            if hash_ in d:
                nodes_with_same_hash.append(d[hash_])
            else:
                break
        if len(nodes_with_same_hash) == length:
            # Cartesian product of each group
            combinations.extend(product(*nodes_with_same_hash))

    return combinations

def calculate_depth_proximity(trees, combination):
    """
    @brief Calculates how close in depth the nodes of a combination are across trees.
    @param trees A list of Trees.
    @param combination A list of nodes (one from each tree).
    @return A depth proximity factor (float).
    """
    depths = []
    for index, node in enumerate(combination):
        depths.append(trees[index].depth(node.identifier))
    span = max(depths) - min(depths)
    return 1 / (span + 1)

def get_ancestors(tree, node):
    ancestors = []
    parent = tree.parent(node.identifier)
    while parent is not None:
        ancestors.append(parent)
        parent = tree.parent(parent.identifier)
    return ancestors

def calculate_environment_similarity(trees, combination):
    """
    @brief Calculates an environmental similarity measure including ancestor and sibling similarities.
    """

    def calculate_sibling_similarity(trees, combination):
        sibling_hashes_per_node = []
        for index, node in enumerate(combination):
            sibs = trees[index].siblings(node.identifier)
            node_sibling_hashes = [(sib.data.subtree_hash, sib.data.subtree_size) for sib in sibs]
            sibling_hashes_per_node.append(node_sibling_hashes)

        sibling_similarity = 0
        if not sibling_hashes_per_node:
            return 0

        first_list = sibling_hashes_per_node[0]
        for sibling_hash in first_list:
            if all(sibling_hash[0] in [s[0] for s in lst] for lst in sibling_hashes_per_node[1:]):
                sibling_similarity += sibling_hash[1]

        # A naive normalization
        biggest_parent_size = 1
        for i, n in enumerate(combination):
            parent = trees[i].parent(n.identifier)
            if parent and parent.data:
                if parent.data.subtree_size > biggest_parent_size:
                    biggest_parent_size = parent.data.subtree_size
        return sibling_similarity / biggest_parent_size

    ancestors_per_node = [get_ancestors(trees[index], node) for index, node in enumerate(combination)]
    direct_sibling_similarity = calculate_sibling_similarity(trees, combination)
    equal_ancestors = 0
    ancestor_sibling_similarities = 0

    min_len = min(len(a) for a in ancestors_per_node) if ancestors_per_node else 0
    for i in range(min_len):
        ancestor_pairing = [ancestors[i] for ancestors in ancestors_per_node]
        first_tag = ancestor_pairing[0].tag
        if all(anc.tag == first_tag for anc in ancestor_pairing):
            equal_ancestors += 1
            ancestor_sibling_similarities += calculate_sibling_similarity(trees, ancestor_pairing)
        else:
            break

    longest_ancestor_chain = max(map(len, ancestors_per_node)) if ancestors_per_node else 1
    ancestor_similarity = equal_ancestors / longest_ancestor_chain if longest_ancestor_chain else 0
    ancestor_sibling_similarity = ancestor_sibling_similarities / (equal_ancestors + 1) if (equal_ancestors + 1) != 0 else 0

    environment_similarity = 0.2 * ancestor_similarity + 0.5 * direct_sibling_similarity + 0.3 * ancestor_sibling_similarity
    return environment_similarity

def calculate_decision_ratio(trees, combination, combinations):
    """
    @brief Calculates a decision ratio factoring in depth proximity, environment similarity, and subtree size.
    """
    depth_proximity = calculate_depth_proximity(trees, combination)
    size = combination[0].data.subtree_size
    environment_similarity = calculate_environment_similarity(trees, combination)
    decision_ratio = 0.2 * depth_proximity + 0.3 * environment_similarity + 0.5 * min((size / 100), 1)
    return decision_ratio

def sort_by_decision_ratio(trees, combinations):
    combination_with_ratio = []
    for combination in combinations:
        ratio = calculate_decision_ratio(trees, combination, combinations)
        pos_sum = -sum(node.data.source_positions[0].value_start_point() for node in combination)
        combination_with_ratio.append((combination, ratio, combination[0].data.subtree_size, pos_sum))

    combination_with_ratio.sort(key=lambda comb: (comb[1], comb[2], comb[3]), reverse=True)
    return [(c[0], c[1]) for c in combination_with_ratio]

def intersect_all_subtrees(tree_groups, options={}):
    """
    @brief Finds all common subtrees among groups of trees.
    @param tree_groups A list of lists of Trees.
    @param options Dictionary of options.
    @return A list of tuples (subtree, ratio).
    """
    if not tree_groups:
        return []

    # Flatten each group by merging them into a single Tree
    merged_trees = []
    for group in tree_groups:
        common_tree = Tree()
        common_root = common_tree.create_node("common_root")
        for t in group:
            common_tree.paste(common_root.identifier, t, False)
        merged_trees.append(common_tree)

    equal_combinations = compute_combinations(merged_trees, options)
    equal_combinations_with_ratio = sort_by_decision_ratio(merged_trees, equal_combinations)
    equal_combinations_with_ratio = remove_overlapping_combinations(equal_combinations_with_ratio)

    matched_subtrees = []
    for (combination, ratio) in equal_combinations_with_ratio:
        first_subtree = merged_trees[0].subtree(combination[0].identifier)
        all_nodes_first = first_subtree.all_nodes()
        for index in range(1, len(combination)):
            subtree = merged_trees[index].subtree(combination[index].identifier)
            all_nodes = subtree.all_nodes()
            # Merge source positions into first_subtree
            for node_first, node_other in zip(all_nodes_first, all_nodes):
                spf = node_first.data.source_positions
                sps = node_other.data.source_positions
                for position in sps:
                    if position not in spf:
                        spf.append(position)
        matched_subtrees.append((first_subtree, ratio))

    return matched_subtrees

def remove_subtree(tree, node):
    """
    @brief Removes a subtree from the given tree starting from a given node upward.
    @return A list of sibling subtrees and any removed ancestors.
    """
    result = []
    siblings = tree.siblings(node.identifier)
    sibling_trees = [tree.subtree(sib.identifier) for sib in siblings]
    result.extend(sibling_trees)

    parent = tree.parent(node.identifier)
    if parent is not None:
        result.extend(remove_subtree(tree, parent))

    return result

def subtraction(leftSide, leftSideIntersected, treesToSubtract, options={}):
    """
    @brief Computes which positions to subtract from leftSide.
    """
    all_intersections = []
    for rightSideTree in treesToSubtract:
        intersections = intersect_all_subtrees(leftSide + [[rightSideTree]], options)
        all_intersections.extend(intersections)

    all_intersections.sort(key=lambda x: x[1], reverse=True)
    # Keep just the trees
    all_intersections = [x[0] for x in all_intersections]
    remove_overlapping(all_intersections)
    intersections_without_overlaps = all_intersections

    all_positions_to_subtract = []
    for intersection in intersections_without_overlaps:
        root_data = intersection.get_node(intersection.root).data
        for pos in root_data.source_positions:
            all_positions_to_subtract.append(pos)

    return all_positions_to_subtract

def difference(leftSide, rightSide, options={}):
    """
    @brief Computes the difference between sets of trees by subtracting rightSide from leftSide.
    @param leftSide A list of lists of Trees.
    @param rightSide A list of Trees to subtract.
    @param options Dictionary of analysis options.
    @return A tuple (leftSideIntersected, positions_to_subtract).
    """
    leftSideIntersectedWithDecisionRatio = intersect_all_subtrees(deepcopy(leftSide), options)
    leftSideIntersected = [x[0] for x in leftSideIntersectedWithDecisionRatio]
    positions_to_subtract = subtraction(leftSide, leftSideIntersected, rightSide, options)
    return (leftSideIntersected, positions_to_subtract)

def print_tree(tree):
    """
    @brief Prints a tree to stdout.
    """
    if tree.size() > 0:
        for node_id in tree.expand_tree():
            node = tree.get_node(node_id)
            depth = tree.depth(node_id)
            indent = "    " * depth
            source_pos_str = [x.render() for x in node.data.source_positions]
            print(f"{indent}{node.data.type} {node.data.text} {node.data.subtree_hash} {source_pos_str}")
        print("\n------------------------------------------------\n")

def print_trees(trees):
    """
    @brief Prints multiple trees, each separated by a line.
    """
    print("\n")
    for tree in trees:
        print_tree(tree)
