# position.py
"""
@file position.py
@brief Contains the SourcePosition class, representing a source-code position range.
"""

from tree_sitter import Point

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
        return (f"{self.file}:"
                f"{self.start_point.row + 1}/{self.start_point.column + 1}-"
                f"{self.end_point.row + 1}/{self.end_point.column + 1}")

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
        return self.start_point.row * 1000 + self.start_point.column
