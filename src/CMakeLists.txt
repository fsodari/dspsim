add_library(dspsim-core STATIC
    ${DSPSIM_SRC_DIR}/clock.cpp
    ${DSPSIM_SRC_DIR}/context.cpp
    ${DSPSIM_SRC_DIR}/coro.cpp
    ${DSPSIM_SRC_DIR}/event.cpp
    ${DSPSIM_SRC_DIR}/model.cpp
    ${DSPSIM_SRC_DIR}/module_name.cpp
    ${DSPSIM_SRC_DIR}/module.cpp
    ${DSPSIM_SRC_DIR}/port.cpp
    ${DSPSIM_SRC_DIR}/process.cpp
    # ${DSPSIM_SRC_DIR}/sensitivity_list.cpp
    ${DSPSIM_SRC_DIR}/signal.cpp)

add_library(dspsim::dspsim-core ALIAS dspsim-core)
set_target_properties(dspsim-core PROPERTIES 
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)

target_compile_features(dspsim-core PUBLIC cxx_std_23)

# Trace logging in debug mode only.
target_compile_definitions(dspsim-core PUBLIC
    "$<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>"
    "$<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>")

# Specific warnings as errors
# -Werror=reorder, /C5038. Modules must be initialized in same order as declaration.
target_compile_options(dspsim-core PRIVATE 
    # GCC and Clang syntax
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Werror=reorder
        -Wall
        -Werror
        -Wextra
        -Wpedantic
        -Wunused
        -Wunused-but-set-variable
        >
    
    # MSVC syntax: /weNNNN (where NNNN is the 4-digit warning number)
    $<$<CXX_COMPILER_ID:MSVC>:
        /W2;>
)

# Link to spdlog
target_link_libraries(dspsim-core PRIVATE $<BUILD_LOCAL_INTERFACE:spdlog::spdlog_header_only>)

# Include dirs
target_include_directories(dspsim-core PUBLIC
    $<BUILD_INTERFACE:${DSPSIM_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>)

# Public headers
target_sources(dspsim-core
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS ${DSPSIM_INCLUDE_DIR}
    FILES 
        ${DSPSIM_INCLUDE_DIR}/dspsim/clock.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/context.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/coro.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/dspsim.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/event.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/model.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/module_name.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/module.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/ndarray.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/port.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/process.h
        # ${DSPSIM_INCLUDE_DIR}/dspsim/sensitivity_list.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/signal.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/modules/axis_rx.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/modules/axis_tx.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/modules/dff.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/flagged_stack.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/priority_queue.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/unique_stack.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/utils.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/vmodule/vmodule.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/vmodule/vport.h)
