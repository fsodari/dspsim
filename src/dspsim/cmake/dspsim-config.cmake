include_guard(GLOBAL)

message("dspsim-config.cmake")

# cmake directory
cmake_path(GET CMAKE_CURRENT_LIST_FILE PARENT_PATH DSPSIM_CMAKE_DIR)
# Package directory
cmake_path(GET DSPSIM_CMAKE_DIR PARENT_PATH DSPSIM_PKG_DIR)
# C++ include directory
set(DSPSIM_INCLUDE_DIRS ${DSPSIM_PKG_DIR}/include)
# HDL directory
set(DSPSIM_HDL_DIR ${DSPSIM_PKG_DIR}/hdl)

message("DSPSim Pkg Dir: ${DSPSIM_PKG_DIR}")
message("DSPSim CMake Dir: ${DSPSIM_CMAKE_DIR}")
message("DSPSim Include Dir: ${DSPSIM_INCLUDE_DIRS}")
message("DSPSim CMake Dir: ${DSPSIM_CMAKE_DIR}")

# Build the dspsim library (once)
function(dspsim_build_library TARGET_NAME)
    message("dspsim_build_library()...")
    #
    if (TARGET ${TARGET_NAME})
        return()
    endif()

    # dspsim library
    add_library(${TARGET_NAME}
        ${DSPSIM_PKG_DIR}/include/dspsim/dspsim.cpp)
    # dspsim include directory
    target_include_directories(${TARGET_NAME} PUBLIC
        ${DSPSIM_PKG_DIR}/include)
endfunction()

function(dspsim_add_module name)
    message("dspsim_add_module()...")

    set(options OPTIONAL SHARED TRACE TRACE_FST)
    set(oneValueArgs SOURCE RENAME)
    set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")

    # Build the dspsim library
    dspsim_build_library(dspsim)

    # Create the nanobind module
    nanobind_add_module(${name} ${arg_UNPARSED_ARGUMENTS}
        STABLE_ABI)

    # Link to the dspsim library.
    target_link_libraries(${name} PRIVATE dspsim)

    # verilate
    verilate(${name} ${arg_TRACE} ${arg_TRACE_FST}
        SOURCES ${arg_SOURCE})

endfunction()