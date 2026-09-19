include_guard(GLOBAL)

set(DSPSIM_GENERATE_CMD ${Python_EXECUTABLE} -m dspsim.framework.generate)

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

    # # Generate stub with nanobind. Do this at install time so that it can find the dspsim._framework module and get the types from it.
    # if (WIN32)
    #     # set(PYTHON_PATH "${CMAKE_INSTALL_PREFIX}/${SKBUILD_PROJECT_NAME}")
    #     set(PYTHON_PATH "$<TARGET_FILE_DIR:${name}>")
    # else()
    #     set(PYTHON_PATH "$<TARGET_FILE_DIR:${name}>")
    # endif()
    set(PYTHON_PATH "$<TARGET_FILE_DIR:${name}>")
    nanobind_add_stub(${name}_stub
        MODULE ${name}
        OUTPUT ${stubs_dir}/${name}.pyi
        PYTHON_PATH ${PYTHON_PATH}
        MARKER_FILE ${marker_file}
        INSTALL_TIME
    )
endfunction()

# Generate a dspsim module with nanobind bindings and Verilated models.
function(dspsim_add_module name pyproject_path output_dir)
    set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/${name}.dir)
    # Install dspsim_generate module
    dspsim_generate(
        ${pyproject_path}
        ${gen_dir})

    nanobind_add_module(${name}
        NB_DOMAIN dspsim
        BACKEND_MODULE nanobind_backend
        NB_DOMAIN dspsim
        ${gen_dir}/${name}.cpp)
    target_link_libraries(${name} PRIVATE dspsim::dspsim-core)

    # Verilated models
    include(${gen_dir}/${name}_include.cmake)

    # Install extension
    install(TARGETS ${name}
        LIBRARY DESTINATION ${output_dir})

    # Generate stubs for the module
    dspsim_add_stub(${name} ${output_dir})
endfunction()

macro(dspsim_update_rpath)
    
endmacro()
