add_executable(DFS_Metadata main.cpp)

file(GLOB_RECURSE DFS_METADATA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

target_sources(DFS_Metadata PRIVATE ${DFS_METADATA_SOURCES})

target_include_directories(DFS_Metadata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(DFS_Metadata PRIVATE DFS_Common)

target_compile_options(DFS_Metadata PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
)