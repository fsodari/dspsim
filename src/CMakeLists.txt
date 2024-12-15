
# Add dspsim-core C++ library. Using shared library. Why? Will static work?
add_library(dspsim-core
    ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/signal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/axis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wishbone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dspsim.cpp)
add_library(dspsim::dspsim-core ALIAS dspsim-core)

# Needed to link to shared libraries.
set_target_properties(dspsim-core PROPERTIES POSITION_INDEPENDENT_CODE On)

# Include Directories.
target_include_directories(dspsim-core PUBLIC
    "$<BUILD_INTERFACE:${DSPSIM_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>")

