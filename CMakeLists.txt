cmake_minimum_required(VERSION 3.25)
project(WideDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global definitions for external libraries to work without exceptions
add_compile_definitions(SPDLOG_NO_EXCEPTIONS GTEST_HAS_EXCEPTIONS=0)

include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)

# Fetch GTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# Fetch xxHash
FetchContent_Declare(
    xxHash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG        v0.8.2
)
FetchContent_MakeAvailable(xxHash)

# Fetch Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
)
FetchContent_MakeAvailable(googlebenchmark)

# WideDB core library
add_library(widedb_core STATIC)

# Include directories
target_include_directories(widedb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${xxhash_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(widedb_core PUBLIC
    spdlog::spdlog
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(widedb_core PUBLIC numa uring)
endif()

# Source files
target_sources(widedb_core PRIVATE
    src/io/io_buffer.cpp
    src/io/io_uring_context.cpp
    src/io/event_loop.cpp
    src/io/async_file.cpp
    src/storage/bloom_filter.cpp
    src/storage/sstable.cpp
)

target_compile_options(widedb_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -fno-exceptions
)

# Tests
enable_testing()
add_subdirectory(tests/unit)
