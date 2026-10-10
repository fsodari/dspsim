#
add_executable(tests
    test_coro.cpp
    test_dff.cpp
    test_hierarchy.cpp
    test_submodule.cpp
    test_adder_tree.cpp
    test_unique_stack.cpp
    test_priority_queue.cpp
    test_better_binding.cpp
    test_print_hierarchy.cpp
    test_clocked_modules.cpp
    test_vmodel.cpp
    test_running_processes.cpp
    test_array.cpp
    test_vmodel_array.cpp
    test_vport.cpp
    test_context.cpp
    test_listeners.cpp
    test_module_name.cpp
    test_dynamic_events.cpp
    test_bitsel.cpp
    test_derived_signal.cpp
    test_signal_packing.cpp)
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

if (MSVC)
    set(TRACE_TYPE TRACE_VCD)
else()
    set(TRACE_TYPE TRACE_FST) 
endif()

add_library(Skid Skid.cpp)
target_link_libraries(Skid PUBLIC dspsim::dspsim-core)
target_link_libraries(tests PRIVATE Skid)

add_library(Skid2 Skid2.cpp)
target_link_libraries(Skid2 PUBLIC dspsim::dspsim-core)
target_link_libraries(tests PRIVATE Skid2)

add_library(NDArrayModel NDArrayModel.cpp)
target_link_libraries(NDArrayModel PUBLIC dspsim::dspsim-core)
target_link_libraries(tests PRIVATE NDArrayModel)

# Verilating. Skip verilating if tests were excluded
if(${verilator_FOUND})
    # Verilate test modules
    verilate(Skid
        SOURCES ${DSPSIM_HDL_DIR}/Skid.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE} 
        VERILATOR_ARGS --quiet)
    verilate(Skid2
        SOURCES ${DSPSIM_HDL_DIR}/Skid.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        PREFIX VSkid2
        VERILATOR_ARGS --quiet)   
    verilate(NDArrayModel
        SOURCES ${DSPSIM_HDL_DIR}/NDArrayModel.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        VERILATOR_ARGS --quiet)
endif()

include(CTest)
include(Catch)
catch_discover_tests(tests)
