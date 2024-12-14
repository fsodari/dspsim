include_guard(GLOBAL)

set(DSPSIM_GENERATE_CMD ${Python_EXECUTABLE} -m dspsim.generate)

function(dspsim_run_generate pyproject_path tool_cfg outdir)
    message("dspsim_run_generate()...")
    
    cmake_path(GET DSPSIM_PKG_DIR PARENT_PATH dspsim_parent)

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

# function(dspsim_basic_module name)
#     # set(options SHARED TRACE TRACE_FST)
#     # set(oneValueArgs CONFIG)
#     # set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)
#     cmake_parse_arguments(PARSE_ARGV 1 arg
#         "${options}" "${oneValueArgs}" "${multiValueArgs}")
#     message("${name}: ${arg_UNPARSED_ARGUMENTS}")

#     # Create framework module
#     nanobind_add_module(${name}
#         STABLE_ABI
#         ${arg_UNPARSED_ARGUMENTS})

#     # Link to dspsim-core library
#     target_link_libraries(${name} PRIVATE dspsim::dspsim-core)

#     # Stub generation
#     nanobind_add_stub(${name}_stub
#         MODULE ${name}
#         OUTPUT ${name}.pyi
#         PYTHON_PATH $<TARGET_FILE_DIR:${name}>
#         MARKER_FILE py.typed
#         DEPENDS ${name})
# endfunction()

function(dspsim_add_module name)
    message("dspsim_add_module()...")

    # set(options SHARED TRACE TRACE_FST)
    set(oneValueArgs CONFIG)
    # set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")

    nanobind_add_module(${name} 
        NB_DOMAIN dspsim
        STABLE_ABI
        # NB_SHARED
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir/${name}.cpp)
    target_link_libraries(${name} PUBLIC dspsim::dspsim-core)

    ### If CONFIG is specified, read in the pyproject.toml config information when building.
    ### This is used when building a dspsim package. Use NO_CONFIG to specify settings in cmake.
    # Use config args?
    if (arg_CONFIG)
        set(pyproject_path ${arg_CONFIG})
    else()
        # default path at root of directory
        set(pyproject_path ${CMAKE_SOURCE_DIR}/pyproject.toml)
    endif()

    set(cfg_path ${CMAKE_CURRENT_BINARY_DIR}/dspsim_tool_cfg.json)
    dspsim_run_generate(${pyproject_path} ${cfg_path} ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir)

    message("Use dspsim_tool_cfg.json")
    # Read the JSON file.
    file(READ ${cfg_path} cfg_json)
    string(JSON lib_type GET ${cfg_json} "libraryType")
    # string(JSON parameters GET ${cfg_json} "parameters")
    # string(JSON include_dirs GET ${cfg_json} "include_dirs")

    # Iterate through all models. Verilate and generate along the way.
    string(JSON models GET ${cfg_json} "models")
    string(JSON n_models LENGTH ${models})
    math(EXPR count "${n_models}-1")
    foreach(IDX RANGE ${count})
        string(JSON model_name MEMBER ${models} ${IDX})
        string(JSON model GET ${models} ${model_name})
        string(JSON model_source GET ${model} "source")
        string(JSON model_parameters GET ${model} "parameters")
        string(JSON _model_include_dirs GET ${model} "includeDirs")
        string(JSON model_trace GET ${model} "trace")
        string(JSON _model_vargs GET ${model} "verilatorArgs")

        # Include dirs
        string(JSON n_include_dirs LENGTH ${_model_include_dirs})
        set(model_include_dirs "")
        if (${n_include_dirs})
            math(EXPR count "${n_include_dirs}-1")            
            foreach(IDX RANGE ${count})
                string(JSON idir GET ${_model_include_dirs} ${IDX})
                list(APPEND model_include_dirs ${idir})
            endforeach()
        endif()

        # verilator_args
        string(JSON n_vargs LENGTH ${_model_vargs})
        set(model_vargs "")
        if (${n_vargs})
            math(EXPR count "${n_vargs}-1")            
            foreach(IDX RANGE ${count})
                string(JSON varg GET ${_model_vargs} ${IDX})
                list(APPEND model_vargs ${varg})
            endforeach()
        endif()

        # Add parameters to vargs
        string(JSON n_params LENGTH ${model_parameters})
        # message(FATAL_ERROR ${model_parameters})
        if (${n_params})
            math(EXPR count "${n_params}-1")            
            foreach(IDX RANGE ${count})
                string(JSON param_id MEMBER ${model_parameters} ${IDX})
                string(JSON param GET ${model_parameters} ${param_id})
                # string(JSON param_name GET ${param} "name")
                # string(JSON param_value GET ${param} "value")
                list(APPEND model_vargs "-G${param_id}=${param}")
            endforeach()
        endif()
        
        if (model_trace STREQUAL "fst")
            set(trace_type TRACE_FST)
        elseif(model_trace STREQUAL "vcd")
            set(trace_type TRACE)
        else()
            set(trace_type "")
        endif()

        set(prefix "V${model_name}")
        set(mdir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir/${model_name}.dir)

        message("Verilating ${model_source}, inc: ${model_include_dirs}, trace: ${trace_type}, prefix ${prefix}, vargs: ${model_vargs}")
        verilate(${name}
            ${trace_type}
            SOURCES ${model_source}
            INCLUDE_DIRS ${model_include_dirs}
            PREFIX "V${model_name}"
            DIRECTORY ${mdir}
            VERILATOR_ARGS ${model_vargs})

        # Generate the model bindings.
    endforeach()
    
    # set_property(TARGET ${name} APPEND PROPERTY BUILD_RPATH "$<TARGET_FILE_DIR:dspsim::dspsim-core>")
    # set_property(TARGET ${name} APPEND PROPERTY BUILD_RPATH "$<TARGET_FILE_DIR:nanobind-abi3-dspsim>")
    # set_property(TARGET ${name} APPEND PROPERTY INSTALL_RPATH "$ORIGIN/lib")    

endfunction()
