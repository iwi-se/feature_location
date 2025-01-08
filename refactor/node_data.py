# node_data.py
"""
@file node_data.py
@brief Contains the NodeData class, which holds metadata about AST nodes.
"""

from position import SourcePosition

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
