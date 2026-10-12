include_guard(GLOBAL)

set(DSPSIM_GENERATE_CMD ${Python_EXECUTABLE} -m dspsim.generate)

# Run the dspsim.generate command.
function(dspsim_generate pyproject_path outdir)
    message(DEBUG "dspsim_generate()...")
    message(${DSPSIM_GENERATE_CMD})
    # Add custom command? This would need to rerun whenever any verilog module changes.. hmm.
    execute_process(COMMAND ${DSPSIM_GENERATE_CMD}
        --pyproject ${pyproject_path}
        --output-dir ${outdir}
        RESULT_VARIABLE gen_result)
    if (gen_result)
        message(FATAL_ERROR "DSPSIM Generate Script failed. ${gen_result}")
    endif()
endfunction()

# Add a nanobind module using the dspsim settings. Takes the module name followed by its source files.
function(dspsim_add_nanobind_module name)
    nanobind_add_module(${name}
        # FREE_THREADED
        BACKEND_MODULE nanobind_backend
        NB_DOMAIN dspsim
        ${ARGN})
    target_link_libraries(${name} PRIVATE dspsim::dspsim-core)
    # dspsim does not use C++ modules: skip CMake's per-file module dependency scan (on by default from C++20).
    set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES OFF)
endfunction()

# Check for the lz4 development files that Verilator's FST tracing needs, and add their
# search paths to a verilated target. Verilator links -llz4 without a search path. That
# works when lz4 is in a compiler default location (Linux system packages), but not for
# Homebrew on Apple Silicon (/opt/homebrew) or other prefixes on CMAKE_PREFIX_PATH.
function(dspsim_target_fst_deps target)
    find_path(DSPSIM_LZ4_INCLUDE_DIR lz4.h)
    find_library(DSPSIM_LZ4_LIBRARY lz4)
    if(NOT DSPSIM_LZ4_INCLUDE_DIR OR NOT DSPSIM_LZ4_LIBRARY)
        message(FATAL_ERROR
            "lz4 development files not found. FST tracing of ${target} needs them: "
            "apt install liblz4-dev, dnf install lz4-devel or brew install lz4. "
            "Set CMAKE_PREFIX_PATH if lz4 is installed in a custom location.")
    endif()

    # Leave compiler default directories alone. They are searched anyway.
    if(NOT DSPSIM_LZ4_INCLUDE_DIR IN_LIST CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
        target_include_directories(${target} SYSTEM PUBLIC ${DSPSIM_LZ4_INCLUDE_DIR})
    endif()
    cmake_path(GET DSPSIM_LZ4_LIBRARY PARENT_PATH lz4_library_dir)
    if(NOT lz4_library_dir IN_LIST CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
        target_link_directories(${target} PUBLIC ${lz4_library_dir})
    endif()
endfunction()

# Turn a VHDL entity into SystemVerilog with GHDL, when CMake configures, and set <out_var> to the path of
# the SystemVerilog file, which verilate() takes as a source (docs/Vhdl.md):
#
#   dspsim_vhdl_source(SKID_SV SOURCE hdl/Skid.vhd GENERICS DW=16)
#   verilate(my_models SOURCES ${SKID_SV})
#
# SOURCE is the file of the entity and SOURCES are the other VHDL files of the design, in any order. ENTITY
# names the entity if it is not named like the file, STD is the VHDL standard (08), GENERICS are NAME=VALUE
# pairs and GHDL_ARGS extra arguments for GHDL. The module has the name of the entity. The VHDL files are
# configure dependencies: CMake configures again when one of them changes. Needs GHDL (DSPSIM_GHDL, or ghdl on
# the path) and the dspsim Python package.
function(dspsim_vhdl_source out_var)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE;ENTITY;STD;OUTPUT_DIR" "SOURCES;GENERICS;GHDL_ARGS")
    if(NOT arg_SOURCE)
        message(FATAL_ERROR "dspsim_vhdl_source: SOURCE is required")
    endif()
    if(NOT arg_OUTPUT_DIR)
        set(arg_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/vhdl)
    endif()

    set(sources "")
    foreach(source IN LISTS arg_SOURCE arg_SOURCES)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        list(APPEND sources ${source})
    endforeach()
    set(command ${Python_EXECUTABLE} -m dspsim.ghdl ${sources} --output-dir ${arg_OUTPUT_DIR})
    if(arg_ENTITY)
        list(APPEND command --entity ${arg_ENTITY})
    endif()
    if(arg_STD)
        list(APPEND command --std ${arg_STD})
    endif()
    foreach(generic IN LISTS arg_GENERICS)
        list(APPEND command -g ${generic})
    endforeach()
    foreach(ghdl_arg IN LISTS arg_GHDL_ARGS)
        list(APPEND command --ghdl-arg=${ghdl_arg})
    endforeach()

    execute_process(COMMAND ${command}
        OUTPUT_VARIABLE path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE error
        RESULT_VARIABLE result)
    if(result)
        message(FATAL_ERROR "dspsim_vhdl_source: ${arg_SOURCE}: ${error}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${sources})
    set(${out_var} ${path} PARENT_SCOPE)
endfunction()

# Generate stubs for a module using the standard configuration for stubs.
function(dspsim_add_stub name output_dir)
    # Install stubs differently for editable installs.
    set(stubs_dir ${output_dir})

    if (SKBUILD_STATE STREQUAL "editable")
        # VSCode typing in editable mode works with this.
        set(marker_file ${stubs_dir}/__init__.pyi)
    else()
        # Otherwise, install stubs into the package
        set(marker_file ${stubs_dir}/py.typed)
    endif()

    # Generate stub with nanobind. Do this at install time so that it can find the dspsim._framework module and get the types from it.
    set(PYTHON_PATH "$<TARGET_FILE_DIR:${name}>")
    nanobind_add_stub(${name}_stub
        MODULE ${name}
        OUTPUT ${stubs_dir}/${name}.pyi
        PYTHON_PATH ${PYTHON_PATH}
        MARKER_FILE ${marker_file}
        INSTALL_TIME
        # Private (leading underscore) bindings are typed too.
        INCLUDE_PRIVATE
        # Optional nanobind stubgen pattern files, given as extra arguments.
        PATTERN_FILE ${ARGN}
    )
endfunction()

# Generate a dspsim library module with nanobind bindings of Verilated models and C++ modules.
function(dspsim_add_module name pyproject_path output_dir)
    set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/${name}.dir)
    # Install dspsim_generate module
    dspsim_generate(
        ${pyproject_path}
        ${gen_dir})

    # NOMINSIZE: nanobind compiles an extension for size (-Os) by default, which is right for bindings, but
    # this one also has the code of the C++ modules whose code is in headers.
    dspsim_add_nanobind_module(${name} NOMINSIZE ${gen_dir}/${name}.cpp)

    # Verilated models and C++ modules
    include(${gen_dir}/${name}_include.cmake)

    # Install extension
    install(TARGETS ${name}
        LIBRARY DESTINATION ${output_dir})

    # Generate stubs for the module
    dspsim_add_stub(${name} ${output_dir})
endfunction()
