# html_renderer.py
"""
@file html_renderer.py
@brief Handles HTML-based rendering of feature-location highlights without relying on the 'html' module.
"""

from tree_sitter import Point

green_span = "<span style=\"background: lightgreen;\">"
red_span = "<span style=\"background: lightcoral;\">"
end_span = "</span>"
line_break = "<br>"
html_doc_start = "<!DOCTYPE html><html><head><title>Feature Location</title></head><body><h1>Feature Location</h1>"
html_doc_end = "</body></html>"
location_window_start = "<div style=\"font-family: monospace;\">"
location_window_end = "</div>"

class HtmlRenderer:
    @staticmethod
    def escape_html(text):
        """
        @brief Escapes special HTML characters in a string to prevent HTML injection and ensure correct rendering.
        """
        # Dictionary mapping special characters to their HTML-escaped counterparts
        escape_mapping = {
            "&": "&amp;",
            "<": "&lt;",
            ">": "&gt;",
            "\"": "&quot;",
            "'": "&#39;",
        }
        escaped_text = []
        for char in text:
            if char in escape_mapping:
                escaped_text.append(escape_mapping[char])
            else:
                escaped_text.append(char)
        return ''.join(escaped_text)
    
    @staticmethod
    def point_in_trace_range(file, point, trace_range):
        """
        @brief Checks if a given character position (point) is within a specified trace range in a file.
        
        Args:
            file (str): The name of the file to check.
            point (Point): The character position to check.
            trace_range (TraceRange): The range to compare against.
        
        Returns:
            bool: True if the point is within the trace range, False otherwise.
        """
        if file != trace_range.file:
            return False
        if point.row < trace_range.start_point.row:
            return False
        if point.row > trace_range.end_point.row:
            return False
        if point.row == trace_range.start_point.row and point.column < trace_range.start_point.column:
            return False
        if point.row == trace_range.end_point.row and point.column >= trace_range.end_point.column:
            return False
        return True

    @staticmethod
    def render_headline(headline):
        """
        @brief Renders a headline (h2) in HTML, escaping the text to ensure safe rendering.
        
        Args:
            headline (str): The headline text to render.
        
        Returns:
            str: The HTML string for the headline.
        """
        escaped_headline = HtmlRenderer.escape_html(headline)
        return f"<h2>{escaped_headline}</h2>"

    @staticmethod
    def render_char(char):
        """
        @brief Renders a single character as HTML, escaping if needed. Spaces are rendered as &nbsp;.
        
        Args:
            char (str): The character to render.
        
        Returns:
            str: The HTML string for the character.
        """
        if char == " ":
            return "&nbsp;"
        return HtmlRenderer.escape_html(char)

    @classmethod
    def render_feature_location_system(cls, code_file, list_of_trace_ranges, list_of_subtraction_ranges=None):
        """
        @brief Renders the feature location for a single code file.
        
        Args:
            code_file (str): The path to the code file.
            list_of_trace_ranges (list): A list of trace ranges to highlight.
            list_of_subtraction_ranges (list, optional): A list of trace ranges to subtract (highlight differently).
        
        Returns:
            str: The HTML string representing the feature location.
        """
        if list_of_subtraction_ranges is None:
            list_of_subtraction_ranges = []

        result = ""
        with open(code_file, "r", encoding='utf-8', errors='replace') as f:
            for line_nr, line in enumerate(f):
                for char_nr, char in enumerate(line):
                    ts_point = Point(line_nr, char_nr)
                    # Check if the current character is within any subtraction range
                    if any(cls.point_in_trace_range(code_file, ts_point, trace_range) 
                           for trace_range in list_of_subtraction_ranges):
                        result += red_span + cls.render_char(char) + end_span
                    # Check if the current character is within any trace range
                    elif any(cls.point_in_trace_range(code_file, ts_point, trace_range) 
                             for trace_ranges_per_trace in list_of_trace_ranges
                             for trace_range in trace_ranges_per_trace):
                        result += green_span + cls.render_char(char) + end_span
                    else:
                        result += cls.render_char(char)
                result += line_break
        return cls.render_headline(code_file) + location_window_start + result + location_window_end

    @classmethod
    def render_feature_location(cls, code_files_left, list_of_trace_ranges, code_files_right=None, list_of_subtraction_ranges=None):
        """
        @brief Renders the feature location results for given sets of files.
        
        Args:
            code_files_left (list): List of left-side code files.
            list_of_trace_ranges (list): List of trace ranges for the left-side files.
            code_files_right (list, optional): List of right-side code files.
            list_of_subtraction_ranges (list, optional): List of subtraction trace ranges.
        
        Returns:
            str: The complete HTML document as a string.
        """
        if code_files_right is None:
            code_files_right = []
        if list_of_subtraction_ranges is None:
            list_of_subtraction_ranges = []

        result = ""
        for code_file in code_files_left:
            result += cls.render_feature_location_system(code_file, list_of_trace_ranges, list_of_subtraction_ranges)
        for code_file in code_files_right:
            result += cls.render_feature_location_system(code_file, [], list_of_subtraction_ranges)
        return html_doc_start + result + html_doc_end
