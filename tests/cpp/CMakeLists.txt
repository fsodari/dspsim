find_package(Catch2 3 REQUIRED)
add_executable(tests
    test_dff.cpp
    test_hierarchy.cpp
    test_submodule.cpp
    test_adder_tree.cpp
    test_unique_stack.cpp
    test_sorted_stack.cpp
    test_better_binding.cpp
    test_print_hierarchy.cpp)
target_link_libraries(tests PRIVATE dspsim-core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)

# add_custom_target(all_tests)
# macro(dspsim_add_test name)
#     add_executable(${name} ${name}.cpp)
#     target_link_libraries(${name} PRIVATE dspsim::dspsim-core)
#     add_test(NAME ${name} COMMAND ${name})
#     add_dependencies(all_tests ${name})
# endmacro()

# dspsim_add_test(test_hierarchy)
# dspsim_add_test(test_dff)
# dspsim_add_test(test_submodule)
