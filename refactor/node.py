# node.py
"""
@file node.py
@brief Contains the Node class, representing a single node in the AST.
"""

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
