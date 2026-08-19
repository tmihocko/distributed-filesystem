add_executable(DFS_Storage main.cpp)

file(GLOB_RECURSE DFS_STORAGE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

target_sources(DFS_Storage PRIVATE ${DFS_STORAGE_SOURCES})

target_include_directories(DFS_Storage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(DFS_Storage PRIVATE DFS_Common)

target_compile_options(DFS_Storage PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
)