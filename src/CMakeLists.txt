add_library(dspsim-core SHARED
    ${DSPSIM_SRC_DIR}/context.cpp
    ${DSPSIM_SRC_DIR}/model.cpp
    ${DSPSIM_SRC_DIR}/signal.cpp
    ${DSPSIM_SRC_DIR}/dff.cpp
    ${DSPSIM_SRC_DIR}/clock.cpp
    ${DSPSIM_SRC_DIR}/simulator.cpp)

add_library(dspsim::dspsim-core ALIAS dspsim-core)
set_target_properties(dspsim-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Include dirs
target_include_directories(dspsim-core PUBLIC
    $<BUILD_INTERFACE:${DSPSIM_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>)

# target_include_directories(dspsim-core PUBLIC
#     ${DSPSIM_INCLUDE_DIR})