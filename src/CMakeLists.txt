include(${CMAKE_CURRENT_LIST_DIR}/dspsim-utils.cmake)

# nanobind_build_library(nanobind-static-abi3)

# Add dspsim-core C++ library. Using shared library. Why? Will static work?
add_library(dspsim-core SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/signal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dspsim.cpp)
add_library(dspsim::dspsim-core ALIAS dspsim-core)
# target_link_libraries(dspsim-core PUBLIC fmt)


# Shared libraries.
set_target_properties(dspsim-core PROPERTIES POSITION_INDEPENDENT_CODE On)

# Include Directories.
target_include_directories(dspsim-core PUBLIC
    "$<BUILD_INTERFACE:${DSPSIM_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>")

# Create framework module
nanobind_add_module(_framework
    NB_DOMAIN dspsim
    STABLE_ABI
    _framework.cpp)
# Link to dspsim-core library
target_link_libraries(_framework
    PUBLIC dspsim::dspsim-core)

# Stub generation
nanobind_add_stub(_framework_stub
    MODULE _framework
    OUTPUT _framework.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:_framework>
    DEPENDS _framework)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs
install(FILES 
  ${CMAKE_CURRENT_BINARY_DIR}/_framework.pyi
    DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs in a local directory to help vscode fine type info with editable installs.
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/_framework.pyi
  DESTINATION ${CMAKE_BINARY_DIR}/stubs/dspsim)


# Install nanobind library

# install(TARGETS nanobind-abi3-dspsim
#     LIBRARY DESTINATION ${INSTALL_LIBDIR})
# install(TARGETS nanobind-static-abi3
#     EXPORT nanobind-static-abi3
#     ARCHIVE DESTINATION ${INSTALL_LIBDIR}
#     LIBRARY DESTINATION ${INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${INSTALL_BINDIR}
#     INCLUDES DESTINATION ${INSTALL_INCLUDEDIR})

### Library Module ###
# Read the tool config from pyproject.toml
# Create library module.
dspsim_add_module(_library
    CONFIG ${CMAKE_SOURCE_DIR}/pyproject.toml)

# Install extension
install(TARGETS _library
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs
install(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/_library.pyi
    DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs in a local directory to help vscode fine type info with editable installs.
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/_library.pyi
    DESTINATION ${CMAKE_BINARY_DIR}/stubs/dspsim)
    