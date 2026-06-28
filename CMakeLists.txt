cmake_minimum_required(VERSION 4.3)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


project(PhysicsEngine)

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
    "src"
)

