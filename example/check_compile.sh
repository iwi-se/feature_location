#!/bin/bash

# Directory containing the headers to check (change this if needed)
FILE_DIR=$1

# Find all header files (*.hpp) in the directory
for file in "$FILE_DIR"/*.cpp; do
    echo "Checking $file..."
    
    # Compile each header file using g++ with the -include option
    g++ -std=c++20 $file -o /dev/null
    
    # Check the exit code of the g++ command
    if [ $? -eq 0 ]; then
        echo "$header: Compiled successfully"
    else
        echo "$header: Compilation failed"
    fi
done