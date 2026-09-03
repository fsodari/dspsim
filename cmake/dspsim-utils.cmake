include_guard(GLOBAL)

# Generate stubs for a module using the standard configuration for stubs.
function(dspsim_add_stub name output_dir)
    # Install stubs differently for editable installs.
    if (SKBUILD_STATE STREQUAL "editable")
        # VSCode typing in editable mode works with this.
        set(stubs_dir ${output_dir}-stubs)
        set(marker_file ${stubs_dir}/__init__.pyi)
    else()
        # Otherwise, install stubs into the package
        set(stubs_dir ${output_dir})
        set(marker_file ${stubs_dir}/py.typed)
    endif()

    # Generate stub with nanobind. Do this at install time so that it can find the dspsim._framework module and get the types from it.
    nanobind_add_stub(${name}_stub
        MODULE ${name}
        OUTPUT ${stubs_dir}/${name}.pyi
        PYTHON_PATH $<TARGET_FILE_DIR:${name}>
        MARKER_FILE ${marker_file}
        INSTALL_TIME
        VERBOSE)
endfunction()
