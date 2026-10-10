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

# Add nanobind module usind dspsim settings
function(dspsim_add_nanobind_module name source_file)
    nanobind_add_module(${name}
        # FREE_THREADED
        BACKEND_MODULE nanobind_backend
        NB_DOMAIN dspsim
        ${source_file})
    target_link_libraries(${name} PRIVATE dspsim::dspsim-core)
endfunction()

# Add the lz4 search paths needed by Verilator's FST tracing to a verilated target.
# Verilator links -llz4 without a search path, and Homebrew on Apple Silicon installs
# to /opt/homebrew, which is not on the compiler's default search path.
function(dspsim_target_fst_deps target)
    if(NOT APPLE)
        return()
    endif()
    find_path(DSPSIM_LZ4_INCLUDE_DIR lz4.h)
    find_library(DSPSIM_LZ4_LIBRARY lz4)
    if(NOT DSPSIM_LZ4_INCLUDE_DIR OR NOT DSPSIM_LZ4_LIBRARY)
        message(WARNING "lz4 not found. FST tracing needs it: brew install lz4")
        return()
    endif()
    cmake_path(GET DSPSIM_LZ4_LIBRARY PARENT_PATH lz4_library_dir)
    target_include_directories(${target} SYSTEM PUBLIC ${DSPSIM_LZ4_INCLUDE_DIR})
    target_link_directories(${target} PUBLIC ${lz4_library_dir})
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
    )
endfunction()

# Generate a dspsim library module with nanobind bindings and Verilated models.
function(dspsim_add_module name pyproject_path output_dir)
    set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/${name}.dir)
    # Install dspsim_generate module
    dspsim_generate(
        ${pyproject_path}
        ${gen_dir})

    dspsim_add_nanobind_module(${name} ${gen_dir}/${name}.cpp)

    # Verilated models
    include(${gen_dir}/${name}_include.cmake)

    # Install extension
    install(TARGETS ${name}
        LIBRARY DESTINATION ${output_dir})

    # Generate stubs for the module
    dspsim_add_stub(${name} ${output_dir})
endfunction()
