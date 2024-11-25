# Build dspsim library
dspsim_build_library(dspsim_lib)

# Create framework module
nanobind_add_module(_framework 
    STABLE_ABI
    _framework.cpp)

# Stub generation
nanobind_add_stub(_framework_stub
    MODULE _framework
    OUTPUT _framework.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:_framework>
    MARKER_FILE py.typed
    DEPENDS _framework)

# Link to dspsim library
target_link_libraries(_framework
    PRIVATE dspsim_lib)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_framework.pyi ${CMAKE_CURRENT_BINARY_DIR}/py.typed
    DESTINATION ${SKBUILD_PROJECT_NAME})

### Library Module ###

### Read the tool config from pyproject.toml


# Create library module.
nanobind_add_module(_library 
    STABLE_ABI
    _library.cpp)
    
dspsim_add_module(_library
    CONFIG ${CMAKE_SOURCE_DIR}/pyproject.toml)



# Library stubs
nanobind_add_stub(_library_stub
    MODULE _library
    OUTPUT _library.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:_library>
    DEPENDS _library)

# Link to dspsim library
target_link_libraries(_library
    PRIVATE dspsim_lib)

# Install the extension
install(TARGETS _library
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_library.pyi
    DESTINATION ${SKBUILD_PROJECT_NAME})