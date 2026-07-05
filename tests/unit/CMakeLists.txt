add_executable(widedb_unit_tests)

target_sources(widedb_unit_tests PRIVATE
    test_io_buffer.cpp
    test_async_file.cpp
    test_sstable.cpp
)

target_link_libraries(widedb_unit_tests PRIVATE
    widedb_core
    GTest::gtest_main
    uring
)

target_compile_options(widedb_unit_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -fno-exceptions
)

include(GoogleTest)
gtest_discover_tests(widedb_unit_tests)
