include(GNUInstallDirs)

# Add dspsim-core C++ library. Using shared library. Why? Will static work?
add_library(dspsim-core SHARED
    # ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dspsim.cpp)
# Shared libraries.
set_target_properties(dspsim-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Include Directories.
target_include_directories(dspsim-core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:dspsim/include>")
# Install headers.
install(DIRECTORY include/
    DESTINATION ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_INCLUDEDIR})
# Install HDL Sources
install(DIRECTORY hdl/
    DESTINATION ${SKBUILD_PROJECT_NAME}/hdl)

# Create framework module
nanobind_add_module(_framework
    STABLE_ABI
    _framework.cpp)
# Link to dspsim-core library
target_link_libraries(_framework
    PRIVATE dspsim-core)

# Stub generation
nanobind_add_stub(_framework_stub
    MODULE _framework
    OUTPUT _framework.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:_framework>
    MARKER_FILE py.typed
    DEPENDS _framework)

# Install core library targets.
install(TARGETS dspsim-core 
    EXPORT dspsim-core-targets
    ARCHIVE DESTINATION ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_LIBDIR})

install(EXPORT dspsim-core-targets
    FILE dspsim-core-targets.cmake
    NAMESPACE ${SKBUILD_PROJECT_NAME}::
    DESTINATION ${SKBUILD_PROJECT_NAME}/cmake)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_framework.pyi ${CMAKE_CURRENT_BINARY_DIR}/py.typed
    DESTINATION ${SKBUILD_PROJECT_NAME})

### Library Module ###

### Read the tool config from pyproject.toml


# # Create library module.
# nanobind_add_module(_library 
#     STABLE_ABI
#     _library.cpp)
    
# dspsim_add_module(_library
#     CONFIG ${CMAKE_SOURCE_DIR}/pyproject.toml)



# # Library stubs
# nanobind_add_stub(_library_stub
#     MODULE _library
#     OUTPUT _library.pyi
#     PYTHON_PATH $<TARGET_FILE_DIR:_library>
#     DEPENDS _library)

# # Link to dspsim library
# target_link_libraries(_library
#     PRIVATE dspsim_lib)

# # Install the extension
# install(TARGETS _library
#     LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# # Install stubs
# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_library.pyi
#     DESTINATION ${SKBUILD_PROJECT_NAME})