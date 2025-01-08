# parsing.py
"""
@file parsing.py
@brief Provides Tree-sitter-based parsing logic and transforms them into our AST Tree structure.
"""

from tree_sitter import Language, Parser
from tree import Tree
from node_data import NodeData
from position import SourcePosition

import tree_sitter_java as tsjava
LANGUAGE = Language(tsjava.language())
parser = Parser(LANGUAGE)

def hash_ts_node(ts_node):
    """
    @brief Computes a hash value for a Tree-sitter node.
    @param ts_node A Tree-sitter node.
    @return A hash value representing the node.
    """
    node_key = (ts_node.start_byte, ts_node.end_byte, ts_node.type)
    hash_self = hash(ts_node.type)

    if ts_node.child_count == 0:
        return hash((hash_self, hash(ts_node.text), 0))
    else:
        children_hash = 0
        for child in ts_node.children:
            children_hash ^= hash_ts_node(child)
        return hash((hash_self, children_hash))

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

        only_named = options.get("only_named_nodes", True)
        for index, child in enumerate(ts_node.children):
            if child.is_named or not only_named:
                traverse_and_build(node.identifier, child, position + [index])

    traverse_and_build(None, treesitter_node, position)
    return tree

def read_and_preprocess(filename, options={}):
    """
    @brief Reads a file, parses it with Tree-sitter, and preprocesses it into a Tree.
    @param filename The source file to parse.
    @param options Dictionary of parsing options.
    @return A Tree representing the AST of the file.
    """
    with open(filename, "rb") as f:
        content = f.read()
        ts_tree = parser.parse(content)
        return preprocess(ts_tree.root_node, filename, [0], options)

def read_and_preprocess_code(code_string, filename="in_memory.java", options={}):
    """
    @brief Parses code from an in-memory string and preprocesses it into a Tree.
    @param code_string The source code as a string.
    @param filename A filename to associate with the code for source positioning (default "in_memory.java").
    @param options Dictionary of parsing options.
    @return A Tree representing the AST of the code.
    """
    content = code_string.encode('utf-8', 'replace')
    ts_tree = parser.parse(content)
    return preprocess(ts_tree.root_node, filename, [0], options)

def get_root_node(tree):
    """
    @brief Retrieves the root node of a given Tree.
    @param tree A Tree object.
    @return The root node object of the tree.
    """
    return tree.get_node(tree.root)
