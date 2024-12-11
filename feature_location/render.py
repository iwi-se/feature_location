from tree_sitter import Point
import feature_location as fl
import html

green_span = "<span style=\"background: lightgreen;\">"
red_span = "<span style=\"background: lightcoral;\">"
end_span = "</span>"
line_break = "<br>"
html_doc_start = "<!DOCTYPE html><html><head><title>Feature Location</title></head><body><h1>Feature Location</h1>"
html_doc_end = "</body></html>"
location_window_start = "<div style=\"font-family: monospace;\">"
location_window_end = "</div>"

def point_in_trace_range(file, point, trace_range):
    """
    @brief Checks if a given character position (point) is within a specified trace range in a file.
    @param file The filename being checked.
    @param point A Tree-sitter Point (row, column).
    @param trace_range A SourcePosition object defining a code range.
    @return True if the point is within the trace_range in the given file, False otherwise.
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


def render_headline(headline):
    """
    @brief Renders a headline (h2) in HTML, escaping the text.
    @param headline The headline string.
    @return An HTML string containing the headline.
    """
    return "<h2>" + html.escape(headline) + "</h2>"


def render_char(char):
    """
    @brief Renders a single character as HTML, escaping if needed. Spaces rendered as &nbsp;.
    @param char A single character.
    @return The HTML-escaped character.
    """
    if char == " ":
        return "&nbsp;"
    else:
        return html.escape(char)


def render_feature_location_system(code_file, list_of_trace_ranges, list_of_subtraction_ranges=None):
    """
    @brief Renders the feature location for a single code file.
    @param code_file The filename of the source code.
    @param list_of_trace_ranges A list of lists of SourcePositions representing highlight (feature) ranges.
    @param list_of_subtraction_ranges A list of SourcePositions representing subtraction (difference) ranges.
    @return An HTML string highlighting features and differences in the code.
    """
    if list_of_subtraction_ranges is None:
        list_of_subtraction_ranges = []
    result = ""
    with open(code_file, "r") as f:
        for line_nr, line in enumerate(f):
            for char_nr, char in enumerate(line):
                ts_point = Point(line_nr, char_nr)
                if any(point_in_trace_range(code_file, ts_point, trace_range) for trace_range in list_of_subtraction_ranges):
                    result += red_span
                    result += render_char(char)
                    result += end_span
                elif any(point_in_trace_range(code_file, ts_point, trace_range) for trace_ranges_per_trace in list_of_trace_ranges for trace_range in trace_ranges_per_trace):
                    result += green_span
                    result += render_char(char)
                    result += end_span
                else:
                    result += render_char(char)
            result += line_break
    return render_headline(code_file) + location_window_start + result + location_window_end

def render_feature_location(code_files_left, list_of_trace_ranges, code_files_right=None, list_of_subtraction_ranges=None):
    """
    @brief Renders the feature location results for given sets of files.
    @param code_files_left A list of filenames on the left side (intersected features).
    @param list_of_trace_ranges A list of lists of SourcePositions representing feature highlights for left side files.
    @param code_files_right A list of filenames on the right side (optional), default None.
    @param list_of_subtraction_ranges SourcePositions for differences, default None.
    @return An HTML string with syntax-highlighted code showing features and differences.
    """
    if code_files_right is None:
        code_files_right = []
    if list_of_subtraction_ranges is None:
        list_of_subtraction_ranges = []

    result = ""
    for code_file in code_files_left:
        result += render_feature_location_system(code_file, list_of_trace_ranges, list_of_subtraction_ranges)
    for code_file in code_files_right:
        result += render_feature_location_system(code_file, [], list_of_subtraction_ranges)
    return html_doc_start + result + html_doc_end
