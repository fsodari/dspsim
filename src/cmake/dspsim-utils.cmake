include_guard(GLOBAL)

function(dspsim_run_generate pyproject_path tool_cfg outdir)
    message("dspsim_run_generate()...")
    execute_process(COMMAND ${DSPSIM_GENERATE_CMD}
        --pyproject ${pyproject_path}
        --tool-cfg ${tool_cfg}
        --output-dir ${outdir}
        RESULT_VARIABLE gen_result)
    if (gen_result)
        message(FATAL_ERROR "DSPSIM Generate Script failed")
    endif()
endfunction(dspsim_run_generate)

# Build the dspsim library.
function(dspsim_build_library name)
    # Only build the library once for a given configuration.
    if (TARGET ${name})
        return()
    endif()
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

    # set(options SHARED TRACE TRACE_FST)
    set(oneValueArgs CONFIG)
    # set(multiValueArgs INCLUDE_DIRS CONFIGURATIONS)

    cmake_parse_arguments(PARSE_ARGV 1 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}")

    message("${name}: ${arg_UNPARSED_ARGUMENTS}")

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
        # string(JSON model_parameters GET ${model} "parameters")
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
        
        if (model_trace STREQUAL "fst")
            set(trace_type TRACE_FST)
        elseif(model_trace STREQUAL "vcd")
            set(trace_type TRACE)
        else()
            set(trace_type "")
        endif()

        set(prefix "V${model_name}")
        set(mdir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir/${prefix}.dir)

        verilate(${name}
            ${trace_type}
            SOURCES ${model_source}
            INCLUDE_DIRS ${model_include_dirs}
            PREFIX "V${model_name}"
            VERILATOR_ARGS ${model_vargs} --quiet)

        # Generate the model bindings.
    endforeach()
    
    # Build the dspsim library
    dspsim_build_library("dspsim-${lib_type}")

    # # Create the nanobind module
    # nanobind_add_module(${name} ${arg_UNPARSED_ARGUMENTS}
    #     STABLE_ABI)

    # Link to the dspsim library.
    target_link_libraries(${name} PRIVATE dspsim_lib)

    # # verilate
    # verilate(${name} ${arg_TRACE} ${arg_TRACE_FST}
    #     SOURCES ${arg_SOURCE})

    # # generate
    # dspsim_run_generate(${name}
    #     SOURCE ${arg_SOURCE})

endfunction()
