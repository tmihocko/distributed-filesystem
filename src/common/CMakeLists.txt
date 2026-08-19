add_library(DFS_Common)

file(GLOB_RECURSE DFS_COMMON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

target_sources(DFS_Common PRIVATE ${DFS_COMMON_SOURCES})

target_include_directories(DFS_Common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/dependencies/include"
)

target_link_libraries(DFS_Common PRIVATE yaml-cpp)

target_compile_definitions(DFS_Common PUBLIC
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

target_compile_options(DFS_Common PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
)