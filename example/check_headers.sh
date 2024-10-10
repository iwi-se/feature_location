#!/bin/bash

# Directory containing the headers to check (change this if needed)
HEADER_DIR=$1

# Find all header files (*.hpp) in the directory
for header in "$HEADER_DIR"/*.hpp; do
    echo "Checking $header..."
    
    # Compile each header file using g++ with the -include option
    g++ -std=c++20 -c -include "$header" -x c++ /dev/null
    
    # Check the exit code of the g++ command
    if [ $? -eq 0 ]; then
        echo "$header: Compiled successfully"
    else
        echo "$header: Compilation failed"
    fi
done