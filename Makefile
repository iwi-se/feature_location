# Makefile for feature_location_cpp

# Compiler
CXX := g++

# Build type (debug or release)
BUILD_TYPE ?= debug

# Base compiler flags
BASE_CXXFLAGS := -std=c++20 -Wall -Wextra -I./src -MMD -MP

# Debug and Release specific flags
ifeq ($(BUILD_TYPE),debug)
    CXXFLAGS := $(BASE_CXXFLAGS) -g -O0 -DDEBUG
    BUILD_DIR := debug
else
    CXXFLAGS := $(BASE_CXXFLAGS) -O3 -DNDEBUG
    BUILD_DIR := release
endif

# Library Paths (adjust these paths if your libraries are in different locations)
LDFLAGS := -L/usr/local/lib

# Libraries to link against
LIBS := -lyaml-cpp -ltree-sitter -ltree-sitter-java -ltree-sitter-cpp

# Directories
OBJ_DIR := obj/$(BUILD_DIR)
SRC_DIR := src

# Source Files
SRCS := $(SRC_DIR)/main.cpp \
        $(SRC_DIR)/configuration.cpp \
        $(SRC_DIR)/evaluation.cpp \
        $(SRC_DIR)/parser.cpp \
        $(SRC_DIR)/render.cpp \
        $(SRC_DIR)/set_operations.cpp \
        $(SRC_DIR)/tree.cpp \
        $(SRC_DIR)/argouml_benchmark_results.cpp

# Object Files (prefixed with obj directory)
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

# Dependency Files (prefixed with obj directory)
DEPS := $(OBJS:.o=.d)

# Executable Name (in obj directory)
EXEC := $(OBJ_DIR)/feature_location

# Default Target
all: $(EXEC)

# Debug target
debug:
	$(MAKE) BUILD_TYPE=debug

# Release target
release:
	$(MAKE) BUILD_TYPE=release

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

# Include dependency files
-include $(DEPS)

# Clean Build Artifacts
clean:
	rm -rf obj

# Phony Targets
.PHONY: all clean debug release
