include_guard(GLOBAL)

# Run
set(DSPSIM_GENERATE_CMD ${Python_EXECUTABLE} -m dspsim.generate)

# Run the dspsim.generate command.
function(dspsim_run_generate pyproject_path tool_cfg outdir)
    message(DEBUG "dspsim_run_generate()...")
    
    cmake_path(GET DSPSIM_PKG_DIR PARENT_PATH dspsim_parent)

    # Add custom command? This would need to rerun whenever any verilog module changes.. hmm.
    execute_process(COMMAND ${DSPSIM_GENERATE_CMD}
        --pyproject ${pyproject_path}
        --tool-cfg ${tool_cfg}
        --output-dir ${outdir}
        RESULT_VARIABLE gen_result
        WORKING_DIRECTORY ${dspsim_parent})
    if (gen_result)
        message(FATAL_ERROR "DSPSIM Generate Script failed")
    endif()
endfunction(dspsim_run_generate)

# Create a dspsim library module. Nanobind module, links to dspsim-core, generates bindings for models according to toml config.
function(dspsim_add_module name)
    message(DEBUG "dspsim_add_module()...")

    # set(options STUBS)
    set(oneValueArgs CONFIG STUBS_DIR INSTALL_DIR)
    # set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    # Create the nanobind module.
    nanobind_add_module(${name} 
        NB_DOMAIN dspsim
        STABLE_ABI
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir/${name}.cpp)
    target_link_libraries(${name} PRIVATE dspsim::dspsim-core)

    ### If CONFIG is specified, read in the pyproject.toml config information when building.
    if (arg_CONFIG)
        set(pyproject_path ${arg_CONFIG})
    else()
        # default path at root of directory
        set(pyproject_path ${CMAKE_SOURCE_DIR}/pyproject.toml)
    endif()

    # Convert the pyproject.toml tool config to json so CMake can read it.
    set(cfg_path ${CMAKE_CURRENT_BINARY_DIR}/dspsim_tool_cfg.json)
    # Generate tool_cfg and model bindings.
    dspsim_run_generate(${pyproject_path} ${cfg_path} ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir)

    # Read the JSON tool config file.
    file(READ ${cfg_path} cfg_json)

    # Iterate through all models. Verilate and generate along the way.
    string(JSON models GET ${cfg_json} "models")
    string(JSON n_models LENGTH ${models})
    math(EXPR count "${n_models}-1")
    foreach(idx RANGE ${count})
        # Get model field values.
        string(JSON model_name MEMBER ${models} ${idx})
        string(JSON model GET ${models} ${model_name})
        string(JSON model_source GET ${model} "source")
        string(JSON model_parameters GET ${model} "parameters")
        string(JSON _model_include_dirs GET ${model} "includeDirs")
        string(JSON model_trace GET ${model} "trace")
        string(JSON _model_vargs GET ${model} "verilatorArgs")

        # Get the list of include dirs
        string(JSON n_include_dirs LENGTH ${_model_include_dirs})
        set(model_include_dirs "")
        if (${n_include_dirs})
            math(EXPR count "${n_include_dirs}-1")            
            foreach(idx RANGE ${count})
                string(JSON idir GET ${_model_include_dirs} ${idx})
                list(APPEND model_include_dirs ${idir})
            endforeach()
        endif()

        # Add parameters to verilator_args
        set(model_vargs "")
        string(JSON n_params LENGTH ${model_parameters})
        if (${n_params})
            math(EXPR count "${n_params}-1")            
            foreach(idx RANGE ${count})
                string(JSON param_id MEMBER ${model_parameters} ${idx})
                string(JSON param GET ${model_parameters} ${param_id})
                list(APPEND model_vargs "-G${param_id}=${param}")
            endforeach()
        endif()

        # any extra verilator_args
        string(JSON n_vargs LENGTH ${_model_vargs})
        if (${n_vargs})
            math(EXPR count "${n_vargs}-1")            
            foreach(idx RANGE ${count})
                string(JSON varg GET ${_model_vargs} ${idx})
                list(APPEND model_vargs ${varg})
            endforeach()
        endif()
        
        # Trace types. vcd or fst.
        if (model_trace STREQUAL "fst")
            set(trace_type TRACE_FST)
        elseif(model_trace STREQUAL "vcd")
            set(trace_type TRACE)
        else()
            set(trace_type "")
        endif()

        # Run verilator to generate the C++ model.
        set(prefix "V${model_name}")
        set(mdir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir/${model_name}.dir)
        message(VERBOSE "Verilating ${model_source}, inc: ${model_include_dirs}, trace: ${trace_type}, prefix ${prefix}, vargs: ${model_vargs}")
        verilate(${name}
            ${trace_type}
            SOURCES ${model_source}
            INCLUDE_DIRS ${model_include_dirs}
            PREFIX "V${model_name}"
            DIRECTORY ${mdir}
            VERILATOR_ARGS ${model_vargs})
    endforeach()

    # Optionally install the extension.
    if (arg_INSTALL_DIR)
        install(TARGETS ${name} LIBRARY DESTINATION ${arg_INSTALL_DIR})
    endif()
    
    # Optionally generate stubs.
    if (arg_STUBS_DIR)
        dspsim_add_stub(${name} ${arg_STUBS_DIR})
    endif()
endfunction()

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
        INSTALL_TIME)
endfunction()
