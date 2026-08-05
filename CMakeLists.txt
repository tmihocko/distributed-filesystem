cmake_minimum_required(VERSION 4.3)

project(DistributedDatabase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(dependencies/yaml-cpp)
add_subdirectory(src/client)
add_subdirectory(src/common)