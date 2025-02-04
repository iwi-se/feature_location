cmake_minimum_required(VERSION 3.22)

project(feature_location LANGUAGES CXX)

find_package(yaml-cpp REQUIRED)
find_package(tree-sitter REQUIRED)
find_package(tree-sitter-cpp REQUIRED)

add_executable(feature_location main.cpp src/configuration.cpp src/evaluation.cpp src/parser.cpp src/tree.cpp src/set_operations.cpp src/render.cpp)

target_include_directories(feature_location PRIVATE ${yaml-cpp_INCLUDE_DIRS} ${tree-sitter_INCLUDE_DIRS} ${tree-sitter-cpp_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR}/include)

target_compile_options(feature_location PRIVATE -Wall)#-Wextra -Werror

target_link_libraries(feature_location PRIVATE yaml-cpp ${tree-sitter_LIBRARIES} ${tree-sitter-cpp_LIBRARIES})