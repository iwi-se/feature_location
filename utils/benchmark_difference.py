#!/usr/bin/env python3
"""
Utility script to compare two files and identify lines unique to each file.

This script reads two files, sorts their lines alphabetically, and then calculates
which lines are only in the first file and which are only in the second file.
"""

import argparse
import sys
from pathlib import Path
from typing import List, Set, Tuple


def read_file_lines(file_path: str) -> List[str]:
    """
    Read a file and return its lines as a list with whitespace stripped.
    
    Args:
        file_path: Path to the file to read
        
    Returns:
        List of lines from the file with whitespace stripped
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            return [line.strip() for line in file if line.strip()]
    except Exception as e:
        print(f"Error reading file {file_path}: {e}", file=sys.stderr)
        sys.exit(1)


def get_unique_lines(file1_lines: List[str], file2_lines: List[str]) -> Tuple[List[str], List[str]]:
    """
    Identify lines unique to each file.
    
    Args:
        file1_lines: Lines from the first file
        file2_lines: Lines from the second file
        
    Returns:
        Tuple containing (lines only in file1, lines only in file2)
    """
    set1 = set(file1_lines)
    set2 = set(file2_lines)
    
    only_in_first = sorted(list(set1 - set2))
    only_in_second = sorted(list(set2 - set1))
    
    return only_in_first, only_in_second


def write_results(only_in_first: List[str], only_in_second: List[str], output_prefix: str = None):
    """
    Write comparison results to output files or stdout.
    
    Args:
        only_in_first: Lines that are only in the first file
        only_in_second: Lines that are only in the second file
        output_prefix: Optional prefix for output files
    """
    if output_prefix:
        with open(f"{output_prefix}_only_in_first.txt", 'w', encoding='utf-8') as f:
            f.write('\n'.join(only_in_first))
            if only_in_first:
                f.write('\n')
                
        with open(f"{output_prefix}_only_in_second.txt", 'w', encoding='utf-8') as f:
            f.write('\n'.join(only_in_second))
            if only_in_second:
                f.write('\n')
                
        print(f"Results written to {output_prefix}_only_in_first.txt and {output_prefix}_only_in_second.txt")
    else:
        print("\nLines only in the first file:")
        for line in only_in_first:
            print(line)
            
        print("\nLines only in the second file:")
        for line in only_in_second:
            print(line)


def main():
    parser = argparse.ArgumentParser(description="Compare two files and find unique lines in each.")
    parser.add_argument("file1", help="Path to the first file")
    parser.add_argument("file2", help="Path to the second file")
    parser.add_argument("-o", "--output", help="Prefix for output files")
    args = parser.parse_args()
    
    # Validate file paths
    for file_path in [args.file1, args.file2]:
        if not Path(file_path).is_file():
            print(f"Error: {file_path} is not a valid file", file=sys.stderr)
            sys.exit(1)
    
    # Read and sort lines from both files
    file1_lines = sorted(read_file_lines(args.file1))
    file2_lines = sorted(read_file_lines(args.file2))
    
    # Get unique lines
    only_in_first, only_in_second = get_unique_lines(file1_lines, file2_lines)
    
    # Write or print results
    write_results(only_in_first, only_in_second, args.output)
    
    # Print summary
    print(f"\nSummary:")
    print(f"Total lines in first file: {len(file1_lines)}")
    print(f"Total lines in second file: {len(file2_lines)}")
    print(f"Lines only in first file: {len(only_in_first)}")
    print(f"Lines only in second file: {len(only_in_second)}")
    print(f"Common lines: {len(set(file1_lines).intersection(set(file2_lines)))}")


if __name__ == "__main__":
    main()
