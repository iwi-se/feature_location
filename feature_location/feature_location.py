from copy import deepcopy
from functools import reduce, cmp_to_key
import tree_sitter_cpp as tscpp
import sys as sys
from tree_sitter import Language, Parser
from itertools import product, permutations
from treelib import Node, Tree

minimumFeatureSize = 1

CPP_LANGUAGE = Language(tscpp.language())

parser = Parser(CPP_LANGUAGE)


class SourcePosition:
    def __init__(self, file, start_point, end_point):
        self.file = file
        self.start_point = start_point
        self.end_point = end_point

    def render(self):
        return self.file + ":" + str(self.start_point.row + 1) + "/" + str(self.start_point.column + 1) + "-" + str(self.end_point.row + 1) + "/" + str(self.end_point.column + 1)

    def relative_position(self, other):
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
        return self.file == other.file and self.start_point == other.start_point and self.end_point == other.end_point
    
    def __hash__(self):
        return hash((self.file, self.start_point, self.end_point))


class NodeData:
    def __init__(self, node_type, node_text, source_positions: list[str],
                 tree_positions: list[list[int]], subtree_hash=None, is_ts_leaf=False, indirect=False):
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


def readAndPreprocess(filename):
    with open(filename, "rb") as f:
        content = f.read()
        tree = parser.parse(content)
        treePreprocessed = preprocess(tree.root_node, filename, [0])
        return treePreprocessed


def hasLeafs(tree):
    potential_leaves = tree.leaves
    return any(node.data.is_ts_leaf for node in potential_leaves)


def hasIdentifier(tree):
    potential_identifiers = tree.leaves
    return any(child.data.type == "identifier" for child in potential_identifiers)


def positions_do_not_cross(positions, other_positions_list):
    if len(other_positions_list) == 0:
        return True
    for other_positions in other_positions_list:
        result = []
        for position, other_position in zip(positions, other_positions):
            result.append(position.relative_position(other_position))
        if not (result[0] != 0 and all(x == result[0] for x in result)):
            return False
    return True


intersectionResultStore = {}


def intersection(trees):
    if not trees:
        return None
    if tuple(trees) in intersectionResultStore:
        return intersectionResultStore[tuple(trees)]

    root_nodes = [tree.get_node(tree.root) for tree in trees]

    # Check for IndirectDescendants
    if any(root_node.data.indirect for root_node in root_nodes):
        result = largestNonOverlappingIntersections(trees)
        result.data.indirect = True
        return result

    # Check if all trees are of the same type
    first_type = root_nodes[0].data.type
    if any(root_node.data.type != first_type for root_node in root_nodes):
        return largestNonOverlappingIntersections(trees)

    if all(root_node.data.is_ts_leaf for root_node in root_nodes):
        first_text = root_nodes[0].data.text
        if not all(root_node.data.text == first_text for root_node in root_nodes):
            return None

    # Generate all combinations of indices for the children of each node
    tl_child_identifiers_per_tree = []
    for tree in trees:
        tl_identifiers = []
        for child_node in tree.children(tree.root):
            tl_identifiers.append(child_node.identifier)
        tl_child_identifiers_per_tree.append(tl_identifiers)

    nodeCombinationSubtrees = []
    for combination in product(*tl_child_identifiers_per_tree):
        # Fetch the corresponding child from each node based on the current combination of indices
        child_subtrees = [trees[i].subtree(combination[i])
                          for i in range(len(trees))]
        # Apply intersection recursively to the list of children
        result = intersection(child_subtrees)
        nodeCombinationSubtrees.append((combination, result))

    def findResultSubtrees(nodeCombinationSubtrees):
        if len(nodeCombinationSubtrees) == 0:
            return []
        resultSubtrees = []
        biggestSubtree = None
        biggestSize = 0
        for indexList, subtree in nodeCombinationSubtrees:
            if subtree is not None:
                if subtree.size() > biggestSize:
                    biggestSize = subtree.size()
                    biggestSubtree = (indexList, subtree)

        if biggestSubtree is not None:
            before = []
            for indexList, subtree in nodeCombinationSubtrees:
                if all(index < big_index for index, big_index in zip(indexList, biggestSubtree[0])):
                    before.append((indexList, subtree))

            after = []
            for indexList, subtree in nodeCombinationSubtrees:
                if all(index > big_index for index, big_index in zip(indexList, biggestSubtree[0])):
                    after.append((indexList, subtree))

            resultSubtrees.extend(findResultSubtrees(before))
            resultSubtrees.append(biggestSubtree[1])
            resultSubtrees.extend(findResultSubtrees(after))

        # if biggestSubtree == None:
        #     treesForLargestIntersections = [[] for _ in trees]
        #     for indexList, subtree in nodeCombinationSubtrees:
        #         for treeIndex, childIndex in enumerate(indexList):
        #             treesForLargestIntersections[treeIndex].append(
        #                 trees[treeIndex].children[childIndex])
        #     largestNonOverlappingIntersectionsL = largestNonOverlappingIntersections(
        #             treesForLargestIntersections)
        #     if len(largestNonOverlappingIntersectionsL) > 0:
        #         for lnoi in largestNonOverlappingIntersectionsL:
        #             resultSubtrees.append(IndirectDescendant(lnoi))

        return resultSubtrees

    resultSubtrees = findResultSubtrees(nodeCombinationSubtrees)

    result_node_data = NodeData(root_nodes[0].data.type, root_nodes[0].data.text if not trees[0].size() > 1 else "".encode(),
                                [source_position for root_node in root_nodes for source_position in root_node.data.source_positions],
                                [tree_position for root_node in root_nodes for tree_position in root_node.data.tree_positions], root_nodes[0].data.is_ts_leaf)

    new_tree = Tree()
    new_root_node = new_tree.create_node(
        result_node_data.type, None, None, result_node_data)
    for subtree in resultSubtrees:
        new_tree.paste(new_root_node.identifier, subtree, False)
    intersectionResultStore[tuple(trees)] = new_tree
    return new_tree


def findIn(nodeToFind, treeToFindNodeIn):
    if containsOneOf(nodeToFind.identifiers, treeToFindNodeIn.identifiers):
        return treeToFindNodeIn
    else:
        for child in treeToFindNodeIn.children:
            result = findIn(nodeToFind, child)
            if result is not None:
                return result
        return None


def subtraction(leftSide, nodesToSubtract):
    allSubtractedIdentifiers = set()

    def markSubtract(currentNode, subtractTree):
        # Begonnen mit TODO siehe unten, aber das ist es noch nicht ganz, prüfe auf die Identifiers vom subtractTree und nicht vom currentNode. Oder?
        if containsOneOf(currentNode.identifiers, subtractTree.allIdentifiers()) and not containsOneOf(currentNode.identifiers, allSubtractedIdentifiers):
            currentNode.subtractMarker = True
            allSubtractedIdentifiers.update(currentNode.identifiers)
        if isinstance(currentNode, IndirectDescendant):
            markSubtract(currentNode.tree, subtractTree)
        else:
            for child in currentNode.children:
                markSubtract(child, subtractTree)

    removeMarkedNodesResult = []

    def removeMarkedNodes(currentNode, parentIsNotSubtract):
        if isinstance(currentNode, IndirectDescendant):
            removeMarkedNodes(currentNode.tree, False)
        elif currentNode.subtractMarker:
            for el in currentNode.children:
                removeMarkedNodes(el, False)
        elif not currentNode.subtractMarker and parentIsNotSubtract:
            children = [removeMarkedNodes(el, True)
                        for el in currentNode.children]
            tree = Tree(currentNode.type, currentNode.text, currentNode.identifiers,
                        currentNode.positions, currentNode.isLeaf())
            tree.children = children
            return tree
        else:
            children = [removeMarkedNodes(el, True)
                        for el in currentNode.children]
            tree = Tree(currentNode.type, currentNode.text, currentNode.identifiers,
                        currentNode.positions, currentNode.isLeaf())
            tree.children = children
            removeMarkedNodesResult.append(tree)

    result = deepcopy(leftSide)
    for index, el in enumerate(nodesToSubtract):
        i = intersection([result, el])
        print("------------------------------------------------Intersection " + str(index) + ":")
        printTreeComplex(i, 0, True)
        markSubtract(result, i)

    print("------------------------------------------------LeftSide:")
    printTreeComplex(leftSide, 0, True)

    print("------------------------------------------------Marked Nodes:")
    printTreeComplex(result, 0, False)

    print("------------------------------------------------Result:")

    removeMarkedNodes(result, False)
    return removeMarkedNodesResult


def removeIndirectDescendants(tree):
    if type(tree) == IndirectDescendant:
        return None
    else:
        newChildren = []
        for child in tree.children:
            newChild = removeIndirectDescendants(child)
            if newChild is not None:
                newChildren.append(newChild)
        tree.children = newChildren
        return tree


lnoiResultStore = {}


def largestNonOverlappingIntersections(trees: list[Tree]):

    if str(trees) in lnoiResultStore:
        return lnoiResultStore[tuple(trees)]

    # Get all nodes from trees
    all_nodes = []
    for tree in trees:
        all_nodes.append(tree.all_nodes())

    # Generate all possible combinations of nodes across different trees
    all_combinations = product(*all_nodes)

    # Compute intersections for each combination
    intersections = []
    for combination in all_combinations:
        intersection_result = intersection(list(combination))
        if intersection_result:
            intersections.append(intersection_result)

    # Sort intersections by size in descending order
    intersections.sort(key=lambda x: x.size(), reverse=True)

    # Select the largest non-overlapping intersection that satisifies the criteria
    for intersection_ in intersections:
        if (hasLeafs(intersection_)
            and hasIdentifier(intersection_)
            and intersection_.size() >= minimumFeatureSize
            ):
            lnoiResultStore[str(trees)] = intersection_
            return intersection_


def intersect_all_subtrees(trees):
    all_nodes = []
    for tree in trees:
        all_nodes.append(tree.all_nodes())

    all_combinations = product(*all_nodes)

    equal_combinations = []
    for combination in all_combinations:
        if all(node.data.subtree_hash == combination[0].data.subtree_hash for node in combination):
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

    result = []
    used_source_positions = set()
    used_root_node_source_positions = []
    for tree in matched_subtrees:
        all_source_positions_in_tree = set()
        all_nodes = tree.all_nodes()
        root_node = tree.get_node(tree.root)
        for node in all_nodes:
            all_source_positions_in_tree.update(node.data.source_positions)
        if (not (all_source_positions_in_tree & used_source_positions)) and positions_do_not_cross(root_node.data.source_positions, used_root_node_source_positions):
            result.append(tree)
            used_source_positions.update(all_source_positions_in_tree)
            used_root_node_source_positions.append(root_node.data.source_positions)

    return result


def printTree(tree):
    # print(tree.show(data_property="rendered",
    #       line_type="ascii", stdout=False, sorting=False))
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


if sys.argv[1] == "intersection":
    trees = []
    for i in range(2, len(sys.argv)):
        trees.append(readAndPreprocess(sys.argv[i]))

    result = intersection(trees)
    printTree(result)

elif sys.argv[1] == "difference":
    treesIntersection = []
    treesSubtraction = []
    afterSeparator = False
    separator = "--"
    for i in range(2, len(sys.argv)):
        if not afterSeparator and sys.argv[i] == separator:
            afterSeparator = True
        elif afterSeparator:
            treesSubtraction.append(readAndPreprocess(sys.argv[i]))
        else:
            treesIntersection.append(readAndPreprocess(sys.argv[i]))

    leftSide = intersection(treesIntersection)

    results = subtraction(leftSide, treesSubtraction)

    for node in results:
        printTreeComplex(node, 0, True)

elif sys.argv[1] == "show_ast":
    trees = []
    for i in range(2, len(sys.argv)):
        trees.append(readAndPreprocess(sys.argv[i]))

    for tree in trees:
        printTree(tree)

elif sys.argv[1] == "test":
    trees = []
    for i in range(2, len(sys.argv)):
        trees.append(readAndPreprocess(sys.argv[i]))

    result = intersect_all_subtrees(trees)
    for tree in result:
        printTree(tree)
