add_library(dspsim-core STATIC
    ${DSPSIM_SRC_DIR}/context.cpp
    ${DSPSIM_SRC_DIR}/model.cpp
    ${DSPSIM_SRC_DIR}/signal.cpp
    ${DSPSIM_SRC_DIR}/dff.cpp
    ${DSPSIM_SRC_DIR}/clock.cpp
    ${DSPSIM_SRC_DIR}/module.cpp
    ${DSPSIM_SRC_DIR}/module_name.cpp
    ${DSPSIM_SRC_DIR}/port.cpp
    ${DSPSIM_SRC_DIR}/sensitivity_list.cpp
    ${DSPSIM_SRC_DIR}/event.cpp)

add_library(dspsim::dspsim-core ALIAS dspsim-core)
set_target_properties(dspsim-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(${LOG_TRACE})
    target_compile_definitions(dspsim-core PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
endif()

# Specific warnings as errors
# -Werror=reorder, /C5038. Modules must be initialized in same order as declaration.
target_compile_options(dspsim-core PUBLIC
    # GCC and Clang syntax
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Werror=reorder;
        -Wall
        -Werror>
    
    # MSVC syntax: /weNNNN (where NNNN is the 4-digit warning number)
    $<$<CXX_COMPILER_ID:MSVC>:
        /W2;>
)

target_link_libraries(dspsim-core PUBLIC spdlog::spdlog)

# Include dirs
target_include_directories(dspsim-core PUBLIC
    $<BUILD_INTERFACE:${DSPSIM_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>)
