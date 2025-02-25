# Makefile for feature_location_cpp

# Compiler
CXX := g++

# Compiler Flags
CXXFLAGS := -std=c++20 -Wall -Wextra -I./src

# Library Paths (adjust these paths if your libraries are in different locations)
LDFLAGS := -L/usr/local/lib

# Libraries to link against
LIBS := -lyaml-cpp -l:libtree-sitter.a -l:libtree-sitter-java.a -l:libtree-sitter-cpp.a -lstdc++fs

# Directories
OBJ_DIR := obj
SRC_DIR := src

# Source Files
SRCS := main.cpp \
        $(SRC_DIR)/configuration.cpp \
        $(SRC_DIR)/evaluation.cpp \
        $(SRC_DIR)/parser.cpp \
        $(SRC_DIR)/render.cpp \
        $(SRC_DIR)/set_operations.cpp \
        $(SRC_DIR)/tree.cpp

# Object Files (prefixed with obj directory)
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

# Executable Name (in obj directory)
EXEC := $(OBJ_DIR)/feature_location

# Default Target
all: $(EXEC)

# Create necessary directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/$(SRC_DIR)

# Link Object Files to Create Executable
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# Compile Source Files to Object Files
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean Build Artifacts
clean:
	rm -rf $(OBJ_DIR)

# Phony Targets
.PHONY: all clean
