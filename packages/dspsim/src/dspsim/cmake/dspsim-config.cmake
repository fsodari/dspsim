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

message("Top Level? ${PROJECT_IS_TOP_LEVEL}")
if (PROJECT_IS_TOP_LEVEL)
    # Run as a script when building the dspsim package
    list(APPEND DSPSIM_GENERATE_CMD ${Python_EXECUTABLE} ${DSPSIM_PKG_DIR}/generate.py)
    # list(APPEND DSPSIM_GENERATE_CMD ${Python_EXECUTABLE} -m dspsim.generate)
else()
    # Run package script if this is installed.
    list(APPEND DSPSIM_GENERATE_CMD dspsim generate)
endif()
message("Generate Command: ${DSPSIM_GENERATE_CMD}")

# Read the pyproject.toml and set the build configuration as env variables.
# If these variables are set, the dspsim functions will get default configs from 
# the env variables. Any options can be manually overridden.
function(dspsim_set_environment)
    message("dspsim_set_environment()...")
    # set(options OPTIONAL SHARED TRACE TRACE_FST)
    # set(oneValueArgs SOURCE RENAME)
    # set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")
    if(NOT arg_UNPARSED_ARGUMENTS)
        set(pyproject_path "pyproject.toml")
    else()
        set(pyproject_path ${arg_UNPARSED_ARGUMENTS})
    endif()
    message("pyproject_path: ${pyproject_path}")

    set(DSPSIM_USE_ENV True)

endfunction()

function(dspsim_run_generate name)
    message("dspsim_run_generate()...")
    set(options OPTIONAL SHARED TRACE TRACE_FST)
    set(oneValueArgs SOURCE RENAME)
    set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")
    execute_process(COMMAND ${DSPSIM_GENERATE_CMD} ${arg_SOURCE}
        RESULT_VARIABLE gen_result)
    if (gen_result)
        message(FATAL_ERROR "DSPSIM Generate Script failed")
    else()
        message("DSPSIM Generate Script Success.")
    endif()

endfunction(dspsim_run_generate)



# Build the dspsim library.
function(dspsim_build_library name)
    # Only build the library once for a given configuration.
    if (TARGET ${name})
        return()
    endif()

    message("dspsim_build_library()...")
    set(options STATIC SHARED)
    # set(oneValueArgs SOURCE RENAME)
    # set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")

    # Read build configuration from pyproject.toml

    # dspsim library
    add_library(${name}
        ${DSPSIM_PKG_DIR}/include/dspsim/dspsim.cpp)
    # dspsim include directory
    target_include_directories(${name} PUBLIC
        ${DSPSIM_PKG_DIR}/include)
endfunction()

function(dspsim_add_module name)
    message("dspsim_add_module()...")

    set(options CONFIG NO_CONFIG SHARED TRACE TRACE_FST)
    set(oneValueArgs SOURCE RENAME)
    set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")

    ### If CONFIG is specified, read in the pyproject.toml config information when building.
    ### This is used when building a dspsim package. Use NO_CONFIG to specify settings in cmake.

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

    # generate
    dspsim_run_generate(${name}
        SOURCE ${arg_SOURCE})

endfunction()