find_package(Catch2 3 REQUIRED)
message(STATUS "Found Catch2: ${Catch2_DIR}")

find_package(verilator REQUIRED)

set(TEST_HDL_MODULES_DIR ${CMAKE_SOURCE_DIR}/tests/test_modules)

add_executable(tests
    test_dff.cpp
    test_hierarchy.cpp
    test_submodule.cpp
    test_adder_tree.cpp
    test_unique_stack.cpp
    test_sorted_stack.cpp
    test_better_binding.cpp
    test_print_hierarchy.cpp
    test_clocked_modules.cpp
    test_vmodel.cpp)
target_link_libraries(tests PRIVATE dspsim-core Catch2::Catch2WithMain)
target_compile_options(tests PRIVATE 
    # GCC and Clang syntax
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Werror=reorder
        -Wall
        -Werror
        -Wno-sign-compare
        -Wno-unused-variable>
    
    # MSVC syntax: /weNNNN (where NNNN is the 4-digit warning number)
    $<$<CXX_COMPILER_ID:MSVC>:
        /W2;>
)
# Verilate test modules
verilate(tests
    SOURCES ${TEST_HDL_MODULES_DIR}/Skid.sv
    INCLUDE_DIRS ${TEST_HDL_MODULES_DIR}
    TRACE_VCD
    VERILATOR_ARGS --quiet
)

include(CTest)
include(Catch)
catch_discover_tests(tests)
