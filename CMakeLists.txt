cmake_minimum_required(VERSION 4.3)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


project(DistributedFilesystem)


# yaml-cpp options
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(dependencies/yaml-cpp)



file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.cc"
)


add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    # $<$<COMPILE_LANGUAGE:CXX>:-O3>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Werror>
	$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>
)

# --- rest of your config unchanged ---
target_include_directories(${PROJECT_NAME} PRIVATE
    "dependencies/include"
    "src"
)

if(TARGET yaml-cpp::yaml-cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp)
else()
    message(FATAL_ERROR "yaml-cpp target not found")
endif()

target_compile_definitions(
	${PROJECT_NAME} 
	PRIVATE 
	ASIO_STANDALONE
	ASIO_NO_DEPRECATED
)