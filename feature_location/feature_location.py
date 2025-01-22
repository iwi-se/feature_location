"""
@file feature_location.py
@brief Provides functionality for feature location through AST analysis and intersection.

This module parses source files, builds ASTs using Tree-sitter, and provides functions to 
compute intersections of common subtrees, differences between sets of ASTs, and render 
source code highlighting.
"""

from copy import deepcopy
import tree_sitter_java as tsjava
import tree_sitter_cpp as tscpp
from functools import lru_cache
from tree_sitter import Language, Parser
from itertools import product
from concurrent.futures import ProcessPoolExecutor, as_completed

# Default configuration options
minimum_trace_size_default = 10
only_named_nodes_default = True
language_default = "cpp"

class SourcePosition:
    """
    @class SourcePosition
    @brief Represents a source code position range in a file.
    """
    def __init__(self, file, start_point, end_point):
        """
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
        return (str(self.file) + ":" +
                str(self.start_point.row + 1) + "/" + str(self.start_point.column + 1) +
                "-" + str(self.end_point.row + 1) + "/" + str(self.end_point.column + 1))

    def relative_position(self, other):
        """
        @brief Compares the relative position of this source segment to another.
        @param other Another SourcePosition object.
        @return An integer indicating relative ordering:
                -1 if self is entirely before other (non-overlapping),
                 0 if overlapping,
                 1 if self is entirely after other.
        """
        sr, sc = self.start_point.row, self.start_point.column
        er, ec = self.end_point.row, self.end_point.column
        osr, osc = other.start_point.row, other.start_point.column
        oer, oec = other.end_point.row, other.end_point.column

        if sr > oer:
            return -1
        if er < osr:
            return 1
        if sr == oer and sc >= oec:
            return -1
        if er == osr and ec <= osc:
            return 1
        return 0

    def __eq__(self, other):
        return (self.file == other.file
                and self.start_point == other.start_point
                and self.end_point == other.end_point)

    def __hash__(self):
        return hash((self.file, self.start_point, self.end_point))

    def value_start_point(self):
        """
        @brief Computes a numeric value for the start point for sorting or comparison.
        @return An integer representing the start location.
        """
        return (self.start_point.row * 1000 + self.start_point.column)


class NodeData:
    """
    @class NodeData
    @brief Holds data for a node in the AST tree including type, text, source positions, and hashes.
    """
    def __init__(self, node_type, node_text, source_positions,
                 tree_positions, subtree_hash=None,
                 is_ts_leaf=False, indirect=False, subtree_size=None, is_named=False):
        """
        @param node_type The type of the AST node.
        @param node_text The raw text associated with this node.
        @param source_positions A list of SourcePosition objects.
        @param tree_positions A list representing the node's position in the tree.
        @param subtree_hash A hash representing the subtree structure.
        @param is_ts_leaf Boolean indicating if this is a Tree-sitter leaf.
        @param indirect Unused boolean flag.
        @param subtree_size The number of descendants in this subtree.
        @param is_named Boolean indicating if the node is named according to Tree-sitter.
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


class Node:
    """
    @class Node
    @brief A simple node structure for the AST tree.
    """
    def __init__(self, identifier, tag, data, parent_id=None):
        self.identifier = identifier
        self.tag = tag
        self.data = data
        self.parent_id = parent_id
        self.children = []


class Tree:
    """
    @class Tree
    @brief A custom tree structure to replace treelib for our AST representation.
    """
    def __init__(self):
        self.nodes = {}
        self._root = None
        self._id_counter = 0

    @property
    def root(self):
        return self._root

    def _generate_id(self):
        self._id_counter += 1
        return f"node_{self._id_counter}"

    def create_node(self, tag, identifier=None, parent=None, data=None):
        if identifier is None:
            identifier = self._generate_id()
        node = Node(identifier, tag, data, parent_id=parent)
        self.nodes[identifier] = node
        if parent is not None:
            self.nodes[parent].children.append(identifier)
        else:
            if self._root is None:
                self._root = identifier
            else:
                # If a root already exists, this node becomes another root-level node.
                # For our use case, this should be acceptable or handled.
                # But we assume we only set root once.
                pass
        return node

    def get_node(self, node_id):
        return self.nodes.get(node_id)

    def parent(self, node_id):
        node = self.get_node(node_id)
        if node and node.parent_id is not None:
            return self.get_node(node.parent_id)
        return None

    def children(self, node_id):
        node = self.get_node(node_id)
        if node:
            return [self.get_node(child_id) for child_id in node.children]
        return []

    def siblings(self, node_id):
        node = self.get_node(node_id)
        if node and node.parent_id is not None:
            parent_node = self.get_node(node.parent_id)
            return [self.get_node(cid) for cid in parent_node.children if cid != node_id]
        return []

    def all_nodes(self):
        return list(self.nodes.values())

    def size(self):
        return len(self.nodes)

    def depth(self, node_id):
        depth = 0
        current = self.get_node(node_id)
        while current and current.parent_id is not None:
            current = self.get_node(current.parent_id)
            depth += 1
        return depth

    def expand_tree(self, mode=None, sorting=False):
        # Simple DFS order
        result = []
        def dfs(node_id):
            result.append(node_id)
            for child_id in self.get_node(node_id).children:
                dfs(child_id)
        if self.root is not None:
            dfs(self.root)
        return result

    def filter_nodes(self, predicate):
        return [n for n in self.all_nodes() if predicate(n)]

    def leaves(self, node_id=None):
        if node_id is None:
            node_id = self.root
        result = []
        def dfs(nid):
            node = self.get_node(nid)
            if node and node.data.is_ts_leaf:
                result.append(node)
            else:
                for cid in node.children:
                    dfs(cid)
        dfs(node_id)
        return result

    def subtree(self, node_id):
        new_tree = Tree()
        # We'll copy all descendants of node_id to new_tree
        # node_id's node becomes the root of new_tree
        def copy_node(nid, parent=None):
            orig_node = self.get_node(nid)
            new_node = new_tree.create_node(orig_node.tag, None, parent, deepcopy(orig_node.data))
            # copy children
            for cid in orig_node.children:
                copy_node(cid, new_node.identifier)
            return new_node
        root_node = copy_node(node_id, None)
        new_tree._root = root_node.identifier
        return new_tree

    def paste(self, parent_id, another_tree, deep=False):
        # Pastes another_tree's children under parent_id node.
        # Shift all nodes of another_tree into this tree and attach the root of another_tree as a child of parent_id.
        # Another_tree root will become a direct child of parent_id.

        # Map from old id to new id
        id_map = {}
        def copy_node(nid, parent):
            orig_node = another_tree.get_node(nid)
            new_node = self.create_node(orig_node.tag, None, parent, deepcopy(orig_node.data))
            id_map[nid] = new_node.identifier
            for cid in orig_node.children:
                copy_node(cid, new_node.identifier)

        if another_tree.root is not None:
            copy_node(another_tree.root, parent_id)

    def __repr__(self):
        return f"<Tree nodes={len(self.nodes)}>"



@lru_cache(maxsize=None)
def hash_ts_node(ts_node):
    """
    @brief Computes a hash value for a Tree-sitter node using an LRU cache for performance.
    @param ts_node A Tree-sitter node.
    @return A hash value representing the node.
    """
    hash_self = hash(ts_node.type)

    if ts_node.child_count == 0:
        return hash((hash_self, hash(ts_node.text), 0))
    else:
        children_hash = 0
        for child in ts_node.children:
            children_hash ^= hash_ts_node(child)
        return hash((hash_self, children_hash))


def get_root_node(tree):
    """
    @brief Retrieves the root node of a given Tree.
    @param tree A Tree object.
    @return The root node object of the tree.
    """
    return tree.get_node(tree.root)


def preprocess(treesitter_node, file, position, options={}):
    """
    @brief Converts a Tree-sitter node into our custom Tree structure with NodeData.
    @param treesitter_node The root Tree-sitter node.
    @param file The filename associated with this node.
    @param position A list representing the tree position, e.g. [0] for root.
    @param options Dictionary of options including 'only_named_nodes'.
    @return A Tree.
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

        only_named = options.get("only_named_nodes", only_named_nodes_default)
        for index, child in enumerate(ts_node.children):
            if child.is_named or not only_named:
                traverse_and_build(node.identifier, child, position + [index])

    traverse_and_build(None, treesitter_node, position)
    return tree


def get_parser(language = language_default):
    if language == "java":
        LANGUAGE = Language(tsjava.language())
        return Parser(LANGUAGE)
    if language == "cpp":
        LANGUAGE = Language(tscpp.language())
        return Parser(LANGUAGE)
    else:
        raise ValueError(f"Unsupported language: {language}")


def _parse_file(filename, options):
    # Helper for parallelization
    with open(filename, "rb") as f:
        content = f.read()
        tree = get_parser(options.get("language", language_default)).parse(content)
        return preprocess(tree.root_node, filename, [0], options)


def read_and_preprocess(filename, options={}):
    """
    @brief Reads a file, parses it with Tree-sitter, and preprocesses it into a Tree.
    @param filename The source file to parse.
    @param options Dictionary of parsing options.
    @return A Tree representing the AST of the file.
    """
    # For a single file, just process it directly
    return _parse_file(filename, options)


def read_and_preprocess_multiple(files, options={}):
    """
    @brief Reads multiple files in parallel, parses them, and returns a list of treelib.Trees.
    """
    trees = []
    if len(files) <= 1:
        for f in files:
            trees.append(read_and_preprocess(f, options))
    else:
        # Parallel execution
        with ProcessPoolExecutor() as executor:
            future_map = {executor.submit(_parse_file, f, options): f for f in files}
            for future in as_completed(future_map):
                trees.append(future.result())
    return trees


def _source_position_to_interval(pos):
    # Convert a SourcePosition to a numeric interval:
    # We'll encode row/column into a single dimension 
    start = pos.start_point.row * 10000000 + pos.start_point.column
    end = pos.end_point.row * 10000000 + pos.end_point.column
    return (start, end)


class IntervalTree:
    """
    A simple Interval Tree implementation to efficiently find overlapping intervals.
    We will insert intervals (start, end) and query if an interval overlaps with any stored ones.
    """

    class Node:
        def __init__(self, start, end):
            self.start = start
            self.end = end
            self.max_end = end
            self.left = None
            self.right = None

    def __init__(self):
        self.root = None

    def insert(self, start, end):
        if self.root is None:
            self.root = IntervalTree.Node(start, end)
        else:
            self._insert(self.root, start, end)

    def _insert(self, node, start, end):
        if start < node.start:
            if node.left is None:
                node.left = IntervalTree.Node(start, end)
            else:
                self._insert(node.left, start, end)
        else:
            if node.right is None:
                node.right = IntervalTree.Node(start, end)
            else:
                self._insert(node.right, start, end)
        node.max_end = max(node.max_end, end)

    def overlaps(self, start, end):
        return self._overlaps(self.root, start, end)

    def _overlaps(self, node, start, end):
        if node is None:
            return False
        if start <= node.end and end >= node.start:
            return True
        if node.left is not None and node.left.max_end >= start:
            return self._overlaps(node.left, start, end)
        return self._overlaps(node.right, start, end)


def read_and_preprocess_code(code_string, filename="in_memory.java", options={}):
    """
    @brief Parses code from an in-memory string and preprocesses it into a Tree.
    @param code_string The source code as a string.
    @param filename A filename to associate with the code for source positioning (default "in_memory.java").
    @param options Dictionary of parsing options.
    @return A Tree representing the AST of the code.
    """
    content = code_string.encode('utf-8', 'replace')
    tree = get_parser(options.get("language", language_default)).parse(content)
    return preprocess(tree.root_node, filename, [0], options)


def positions_do_not_cross(positions, other_positions_list):
    """
    @brief Checks if the given positions do not overlap with any sets of other positions.
    This now uses an Interval Tree for O(log n) overlap checks instead of sorting every time.
    
    @param positions A list of SourcePosition objects.
    @param other_positions_list A list of lists of SourcePosition objects.
    @return True if no crossing is found, False otherwise.
    """
    if not other_positions_list:
        return True

    # Build an interval tree for other_positions_list once
    tree = IntervalTree()
    for opl in other_positions_list:
        for pos in opl:
            start, end = _source_position_to_interval(pos)
            tree.insert(start, end)

    # Check each position against the interval tree
    for p in positions:
        start, end = _source_position_to_interval(p)
        if tree.overlaps(start, end):
            return False

    return True


def remove_overlapping(trees):
    """
    @brief Removes overlapping trees from a list of trees.
    @param trees A list of Trees.
    @return A filtered list without overlaps.
    """
    result = []
    # We'll store accepted intervals in an interval tree to quickly check overlaps
    interval_tree = IntervalTree()

    used_source_positions_flat = []
    for tree in trees:
        all_nodes = tree.all_nodes()
        all_source_positions_in_tree = {pos for node in all_nodes for pos in node.data.source_positions}

        root_node = tree.get_node(tree.root)
        root_positions = root_node.data.source_positions

        # Check overlap with interval tree
        no_overlap = True
        for p in root_positions:
            start, end = _source_position_to_interval(p)
            if interval_tree.overlaps(start, end):
                no_overlap = False
                break

        if no_overlap:
            # Insert these intervals into interval tree for future checks
            for p in all_source_positions_in_tree:
                start, end = _source_position_to_interval(p)
                interval_tree.insert(start, end)
            result.append(tree)

    return result


def remove_overlapping_combinations(combinations):
    """
    @brief Removes overlapping subtree combinations.
    @param combinations A list of tuples (combination, ratio).
    @return A filtered list of combinations.
    """
    result = []
    interval_tree = IntervalTree()

    for (combination, ratio) in combinations:
        all_root_node_source_positions = [pos for node in combination for pos in node.data.source_positions]

        # Check if there's an overlap using the interval tree
        no_overlap = True
        for p in all_root_node_source_positions:
            start, end = _source_position_to_interval(p)
            if interval_tree.overlaps(start, end):
                no_overlap = False
                break

        if no_overlap:
            # Insert them into the interval tree
            for p in all_root_node_source_positions:
                start, end = _source_position_to_interval(p)
                interval_tree.insert(start, end)
            result.append((combination, ratio))

    return result


def remove_subtree(tree, node):
    """
    @brief Removes a subtree from the given tree starting from a given node upward.
    @param tree A Tree.
    @param node A node in the tree.
    @return A list of sibling subtrees and any removed ancestors.
    """
    result = []
    # Siblings
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
    @param leftSide List of lists of Trees for the left side sets.
    @param leftSideIntersected List of Trees representing intersections.
    @param treesToSubtract A list of Trees.
    @param options Dictionary of analysis options.
    @return A list of SourcePosition objects to subtract.
    """
    all_intersections = []
    for rightSideTree in treesToSubtract:
        intersections = intersect_all_subtrees(deepcopy(leftSide) + [deepcopy(rightSideTree)], options)
        all_intersections.extend(intersections)

    all_intersections.sort(key=lambda x: x[1], reverse=True)
    all_intersections = [x[0] for x in all_intersections]

    remove_overlapping(all_intersections)
    intersections_without_overlaps = all_intersections

    all_positions_to_subtract = []
    ap_append = all_positions_to_subtract.append
    for intersection in intersections_without_overlaps:
        for pos in intersection.get_node(intersection.root).data.source_positions:
            ap_append(pos)
    return all_positions_to_subtract


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
    for nodes in all_nodes_per_tree:
        current_dict = {}
        for node in nodes:
            h = node.data.subtree_hash
            current_dict.setdefault(h, []).append(node)
        partition_by_subtree_hash.append(current_dict)

    # Intersect keys to reduce complexity
    common_keys = set(partition_by_subtree_hash[0].keys())
    for d in partition_by_subtree_hash[1:]:
        common_keys.intersection_update(d.keys())

    combinations = []
    for h in common_keys:
        nodes_list = [d[h] for d in partition_by_subtree_hash]
        # Now product only on minimal sets
        combinations.extend(product(*nodes_list))

    return combinations


def sort_by_size_to_remove_ratio(combinations):
    """
    @brief Sort combinations based on size-to-remove ratio.
    @param combinations A list of subtree combinations.
    @return Sorted combinations.
    """
    def calculate_size_to_remove_ratio(combination, combinations):
        own_size = combination[0].data.subtree_size
        own_positions = [pos for node in combination for pos in node.data.source_positions]

        # Create a single interval tree for others to quickly check overlap:
        other_interval_tree = IntervalTree()
        for other_combination in combinations:
            if other_combination is not combination:
                for p in (pos for node in other_combination for pos in node.data.source_positions):
                    start, end = _source_position_to_interval(p)
                    other_interval_tree.insert(start, end)

        # Check overlap once using interval queries
        overlap_found = False
        other_size_total = 0
        for oc in combinations:
            if oc is not combination:
                for p in (pos for node in oc for pos in node.data.source_positions):
                    start, end = _source_position_to_interval(p)
                    # If any position overlaps with own_positions:
                    # we must check each own_position:
                    for own_p in own_positions:
                        os, oe = _source_position_to_interval(own_p)
                        break
                pass

        other_size_total = 0
        for other_combination in combinations:
            if other_combination is not combination:
                other_positions = [pos for node in other_combination for pos in node.data.source_positions]
                if not positions_do_not_cross(own_positions, [other_positions]):
                    other_size_total += other_combination[0].data.subtree_size

        size_to_remove = (10 * own_size) / (other_size_total + 1)
        normalized_size_to_remove = 1 - (size_to_remove / own_size)
        return normalized_size_to_remove

    combination_with_ratio = []
    for combination in combinations:
        ratio = calculate_size_to_remove_ratio(combination, combinations)
        combination_with_ratio.append((combination, ratio))
    combination_with_ratio.sort(key=lambda comb: comb[1], reverse=True)
    return [x[0] for x in combination_with_ratio]


def calculate_depth_proximity(trees, combination):
    """
    @brief Calculates how close in depth the nodes of a combination are across trees.
    @param trees A list of Trees.
    @param combination A list of nodes (one from each tree).
    @return A depth proximity factor (float).
    """
    depths = [trees[index].depth(node.identifier) for index, node in enumerate(combination)]
    span = max(depths) - min(depths)
    return 1 / (span + 1)


def get_ancestors(tree, node):
    """
    @brief Retrieves all ancestors of a given node.
    @param tree A Tree.
    @param node A node in the tree.
    @return A list of ancestor nodes.
    """
    ancestors = []
    parent = tree.parent(node.identifier)
    while parent is not None:
        ancestors.append(parent)
        parent = tree.parent(parent.identifier)
    return ancestors


def calculate_environment_similarity(trees, combination):
    """
    @brief Calculates an environmental similarity measure including ancestor and sibling similarities.
    @param trees A list of Trees.
    @param combination A list of nodes (one from each tree).
    @return A float representing environment similarity.
    """
    def calculate_sibling_similarity(trees, combination):
        sibling_hashes_per_node = []
        for index, node in enumerate(combination):
            sibs = trees[index].siblings(node.identifier)
            node_sibling_hashes = [(sib.data.subtree_hash, sib.data.subtree_size, trees[index].leaves(sib.identifier), sib) for sib in sibs]
            sibling_hashes_per_node.append(node_sibling_hashes)

        sibling_similarity = 0
        if not sibling_hashes_per_node:
            return 0
        first_list = sibling_hashes_per_node[0]

        def getParentSubtreeSize(t):
            tr, nd = t
            p = tr.parent(nd.identifier)
            if p is None or p.data is None:
                return tr.size()
            return p.data.subtree_size

        for sibling_hash in first_list:
            # direct hash match
            if all(any(sibling_hash[0] == s[0] for s in lst) for lst in sibling_hashes_per_node[1:]):
                sibling_similarity += sibling_hash[1]
            else:
                # partial leaf text match scenario
                s_tag = sibling_hash[3].tag
                leaves_text = [l.data.text for l in sibling_hash[2]]
                named_leaves_text = {t for t in leaves_text}

                leave_in_all = True
                for other_list in sibling_hashes_per_node[1:]:
                    matching_sib = [sib for sib in other_list if sib[3].tag == s_tag]
                    if not matching_sib:
                        leave_in_all = False
                        break
                    text_found_all = False
                    for candidate_sib in matching_sib:
                        candidate_leaves_text = {l.data.text for l in candidate_sib[2] if l.data.is_named}
                        if named_leaves_text & candidate_leaves_text:
                            text_found_all = True
                            break
                    if not text_found_all:
                        leave_in_all = False
                        break
                if leave_in_all:
                    sibling_similarity += 1

        parent_sizes = []
        for i, n in enumerate(combination):
            p = trees[i].parent(n.identifier)
            if p and p.data:
                parent_sizes.append(p.data.subtree_size)
            else:
                parent_sizes.append(trees[i].size())

        denom = max(parent_sizes) if parent_sizes else 1
        return sibling_similarity / denom

    ancestors_per_node = [get_ancestors(trees[index], node) for index, node in enumerate(combination)]
    direct_sibling_similarity = calculate_sibling_similarity(trees, combination)
    equal_ancestors = 0
    ancestor_sibling_similarities = 0

    min_len = min([len(a) for a in ancestors_per_node]) if ancestors_per_node else 0
    for i in range(min_len):
        ancestor_pairing = [ancestors[i] for ancestors in ancestors_per_node]
        first_tag = ancestor_pairing[0].tag
        if all(ancestor.tag == first_tag for ancestor in ancestor_pairing):
            equal_ancestors += 1
            ancestor_sibling_similarity_local = calculate_sibling_similarity(trees, ancestor_pairing)
            ancestor_sibling_similarities += ancestor_sibling_similarity_local
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
    @param trees A list of Trees.
    @param combination A combination of nodes.
    @param combinations All combinations.
    @return A float representing the decision ratio.
    """
    depth_proximity = calculate_depth_proximity(trees, combination)
    size = combination[0].data.subtree_size
    environment_similarity = calculate_environment_similarity(trees, combination)
    decision_ratio = 0.2 * depth_proximity + 0.3 * environment_similarity + 0.5 * min((size / 100), 1)
    return decision_ratio


def sort_by_decision_ratio(trees, combinations):
    """
    @brief Sorts combinations by their computed decision ratio.
    @param trees A list of Trees.
    @param combinations A list of node combinations.
    @return A list of tuples (combination, ratio) sorted by the decision ratio.
    """
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
    trees = []
    for group in tree_groups:
        common_tree = Tree()
        common_root = common_tree.create_node("common_root")
        for t in group:
            common_tree.paste(common_root.identifier, t, False)
        trees.append(common_tree)

    equal_combinations = compute_combinations(trees, options)
    equal_combinations_with_ratio = sort_by_decision_ratio(trees, equal_combinations)
    equal_combinations_with_ratio = remove_overlapping_combinations(equal_combinations_with_ratio)

    matched_subtrees = []
    for (combination, ratio) in equal_combinations_with_ratio:
        first_subtree = trees[0].subtree(combination[0].identifier)
        all_nodes_first = first_subtree.all_nodes()
        for index in range(1, len(combination)):
            subtree = trees[index].subtree(combination[index].identifier)
            all_nodes = subtree.all_nodes()
            # Zip could break if different structure, we assume same structure due to hash match.
            for node_first, node in zip(all_nodes_first, all_nodes):
                spf = node_first.data.source_positions
                sps = node.data.source_positions
                tpf = node_first.data.tree_positions
                tps = node.data.tree_positions

                for position in sps:
                    if position not in spf:
                        spf.append(position)
                for position in tps:
                    if position not in tpf:
                        tpf.append(position)
        matched_subtrees.append((first_subtree, ratio))

    return matched_subtrees


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
    @param tree A Tree.
    """
    if tree.size() > 0:
        for node_id in tree.expand_tree():
            node = tree.get_node(node_id)
            depth = tree.depth(node_id)
            print("    " * depth, end="")
            rendered = node.data.type + " " + \
                (str(node.data.text) if node.data.is_ts_leaf else "") + " " + \
                str(node.data.subtree_hash) + " " + \
                str([x.render() for x in node.data.source_positions])
            print(rendered)
        print("\n------------------------------------------------\n")


def print_trees(trees):
    """
    @brief Prints multiple trees, each separated by a line.
    @param trees A list of Trees.
    """
    print("\n")
    for tree in trees:
        print_tree(tree)
