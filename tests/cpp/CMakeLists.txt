find_package(Catch2 3 REQUIRED)
message(STATUS "Found Catch2: ${Catch2_DIR}")

add_executable(tests
    test_dff.cpp
    test_hierarchy.cpp
    test_submodule.cpp
    test_adder_tree.cpp
    test_unique_stack.cpp
    test_sorted_stack.cpp
    test_better_binding.cpp
    test_print_hierarchy.cpp
    test_clocked_modules.cpp)
target_link_libraries(tests PRIVATE dspsim-core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)
