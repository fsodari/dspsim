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
    test_vmodel_wide.cpp
    test_vmodel_struct.cpp
    test_vmodel_struct_array.cpp
    test_vmodel_struct_value.cpp
    test_vport.cpp
    test_context.cpp
    test_listeners.cpp
    test_module_name.cpp
    test_dynamic_events.cpp
    test_bitsel.cpp
    test_derived_signal.cpp
    test_signal_packing.cpp
    test_async.cpp
    test_late_registration.cpp
    test_events.cpp
    test_spawn.cpp
    test_channel.cpp
    test_process_control.cpp
    test_struct.cpp
    test_value_signal.cpp
    test_field_signal.cpp)
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

# The verilated test models are built as one library. verilate() adds Verilator's runtime sources
# (verilated.cpp and the tracing and threading support) to the target it is given, so calling it several
# times on one target compiles the runtime once, where a target per model compiles it once per model.
# All models in a target must use the same trace format: it sets VM_TRACE_VCD / VM_TRACE_FST for the target.
add_library(test_models Skid.cpp Skid2.cpp NDArrayModel.cpp sc_time_stamp.cpp)
target_link_libraries(test_models PUBLIC dspsim::dspsim-core)
target_link_libraries(tests PRIVATE test_models)

if (TRACE_TYPE STREQUAL "TRACE_FST")
    dspsim_target_fst_deps(test_models)
endif()

# Verilating. Skip verilating if tests were excluded
if(${verilator_FOUND})
    # Verilate test modules
    verilate(test_models
        SOURCES ${DSPSIM_HDL_DIR}/Skid.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE} 
        VERILATOR_ARGS --quiet)
    verilate(test_models
        SOURCES ${DSPSIM_HDL_DIR}/Skid.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        PREFIX VSkid2
        VERILATOR_ARGS --quiet)   
    verilate(test_models
        SOURCES ${DSPSIM_HDL_DIR}/NDArrayModel.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        VERILATOR_ARGS --quiet)
    verilate(test_models
        SOURCES ${DSPSIM_HDL_DIR}/WideModel.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        VERILATOR_ARGS --quiet)
    verilate(test_models
        SOURCES ${DSPSIM_HDL_DIR}/StructModel.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        VERILATOR_ARGS --quiet)
    verilate(test_models
        SOURCES ${DSPSIM_HDL_DIR}/StructArrayModel.sv
        INCLUDE_DIRS ${DSPSIM_HDL_DIR}
        ${TRACE_TYPE}
        VERILATOR_ARGS --quiet)
endif()

# A VHDL model, if there is a GHDL to turn it into SystemVerilog with and the dspsim Python package to run it
# (an isolated build of the wheel has neither). The module is called Skid, like the one of Skid.sv: the prefix
# keeps the two verilated models apart.
if(${verilator_FOUND})
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c "import sys, dspsim.ghdl; sys.exit(not dspsim.ghdl.ghdl_available())"
        RESULT_VARIABLE no_ghdl
        OUTPUT_QUIET ERROR_QUIET)
    if(no_ghdl)
        message(STATUS "GHDL not found: the C++ test of a VHDL model is left out")
    else()
        dspsim_vhdl_source(SKID_VHDL_SOURCE SOURCE ${DSPSIM_HDL_DIR}/vhdl/Skid.vhd)
        verilate(test_models
            SOURCES ${SKID_VHDL_SOURCE}
            ${TRACE_TYPE}
            PREFIX VSkidVhdl
            VERILATOR_ARGS --quiet)
        target_sources(tests PRIVATE test_vmodel_vhdl.cpp)
    endif()
endif()

include(CTest)
include(Catch)
catch_discover_tests(tests)
