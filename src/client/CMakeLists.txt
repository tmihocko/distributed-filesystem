add_executable(DFS_Client
    main.cpp
)

file(GLOB_RECURSE DFS_CLIENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

target_sources(DFS_Client PRIVATE
    ${DFS_CLIENT_SOURCES}
)

target_include_directories(DFS_Client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(DFS_Client PRIVATE
    DFS_Common
)

target_compile_options(DFS_Client PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
)