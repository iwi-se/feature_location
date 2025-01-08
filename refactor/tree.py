# tree.py
"""
@file tree.py
@brief A custom tree structure to represent ASTs.
"""
from node import Node

def deepcopy(obj):
    """
    Custom implementation of deepcopy that handles common Python data structures.
    """
    if isinstance(obj, dict):
        # Recursively copy each key-value pair
        new_obj = {}
        for k, v in obj.items():
            new_obj[deepcopy(k)] = deepcopy(v)
        return new_obj
    elif isinstance(obj, list):
        # Recursively copy each element in the list
        return [deepcopy(item) for item in obj]
    elif isinstance(obj, set):
        # Recursively copy each element in the set
        return {deepcopy(item) for item in obj}
    elif isinstance(obj, tuple):
        # Recursively copy each element in the tuple
        return tuple(deepcopy(item) for item in obj)
    # For immutable or user-defined objects that don't need special handling
    return obj

class Tree:
    """
    @class Tree
    @brief A custom tree structure to replace external libraries for our AST representation.
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
            # Set the root if not set
            if self._root is None:
                self._root = identifier
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
        def dfs(nid):
            result.append(nid)
            for child_id in self.get_node(nid).children:
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
            if node and len(node.children) == 0:
                result.append(node)
            else:
                for cid in node.children:
                    dfs(cid)
        dfs(node_id)
        return result

    def subtree(self, node_id):
        new_tree = Tree()
        def copy_node(nid, parent=None):
            orig_node = self.get_node(nid)
            new_node = new_tree.create_node(orig_node.tag, None, parent, deepcopy(orig_node.data))
            for cid in orig_node.children:
                copy_node(cid, new_node.identifier)
            return new_node
        root_node = copy_node(node_id, None)
        new_tree._root = root_node.identifier
        return new_tree

    def paste(self, parent_id, another_tree, deep=False):
        # Pastes another_tree's children under parent_id node.
        def copy_node(nid, parent):
            orig_node = another_tree.get_node(nid)
            new_node = self.create_node(orig_node.tag, None, parent, deepcopy(orig_node.data))
            for cid in orig_node.children:
                copy_node(cid, new_node.identifier)
        if another_tree.root is not None:
            copy_node(another_tree.root, parent_id)

    def __repr__(self):
        return f"<Tree nodes={len(self.nodes)}>"
