cmake_minimum_required(VERSION 4.3)

project(DistributedDatabase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

add_subdirectory(dependencies/yaml-cpp)
add_subdirectory(src/client)
add_subdirectory(src/common)
add_subdirectory(src/metadata)
add_subdirectory(src/storage)