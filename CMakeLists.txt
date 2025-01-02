cmake_minimum_required(VERSION 3.28)
project(${SKBUILD_PROJECT_NAME} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# Required packages.
find_package(Python COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT} REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(verilator CONFIG REQUIRED HINTS $ENV{VERILATOR_ROOT})

message(STATUS "Found Python: ${Python_EXECUTABLE}")
message(STATUS "Python Include: ${Python_INCLUDE_DIRS}")
message(STATUS "Found Nanobind: ${nanobind_DIR}")
message(STATUS "Found Verilator: ${verilator_DIR}")

# Install locations
include(GNUInstallDirs)

# dspsim paths.
set(DSPSIM_PKG_DIR ${CMAKE_SOURCE_DIR}/src/dspsim)
set(DSPSIM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DSPSIM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/include)
set(DSPSIM_HDL_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/hdl)

# Install paths
set(INSTALL_LIBDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_LIBDIR})
set(INSTALL_INCLUDEDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_INCLUDEDIR})
set(INSTALL_DATADIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_DATADIR})
set(INSTALL_CMAKEDIR ${SKBUILD_PROJECT_NAME}/cmake/dspsim)
set(INSTALL_HDLDIR ${SKBUILD_PROJECT_NAME}/hdl)

# Include dspsim utilities
include(${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake)

# Build dspsim-core library.
add_subdirectory(src)

# Util module
nanobind_add_module(_util
    NB_DOMAIN dspsim
    STABLE_ABI
    ${DSPSIM_SRC_DIR}/_util.cpp
)
target_link_libraries(_util PRIVATE dspsim::dspsim-core)
# Install extension
install(TARGETS _util
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs.
dspsim_add_stub(_util ${SKBUILD_PROJECT_NAME})

# Create framework module
nanobind_add_module(_framework
    NB_DOMAIN dspsim
    STABLE_ABI
    ${DSPSIM_SRC_DIR}/_framework.cpp)
# Link to dspsim-core library
target_link_libraries(_framework
    PRIVATE dspsim::dspsim-core)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs.
dspsim_add_stub(_framework ${SKBUILD_PROJECT_NAME})

### Library Module ###
# Create library module using dspsim utilities.
dspsim_add_module(_library
    CONFIG ${CMAKE_SOURCE_DIR}/pyproject.toml
    STUBS_DIR ${SKBUILD_PROJECT_NAME}
    INSTALL_DIR ${SKBUILD_PROJECT_NAME})

# Install core library and export targets.
install(TARGETS dspsim-core
    EXPORT dspsim-core-targets
    ARCHIVE DESTINATION ${INSTALL_LIBDIR}
    LIBRARY DESTINATION ${INSTALL_LIBDIR}
    RUNTIME DESTINATION ${INSTALL_BINDIR}
    INCLUDES DESTINATION ${INSTALL_INCLUDEDIR})

install(EXPORT dspsim-core-targets
    FILE dspsim-core-targets.cmake
    NAMESPACE dspsim::
    DESTINATION ${INSTALL_CMAKEDIR})

# Install CMake package config.
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/dspsim-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
  INSTALL_DESTINATION ${INSTALL_CMAKEDIR}
  PATH_VARS INSTALL_CMAKEDIR INSTALL_LIBDIR INSTALL_INCLUDEDIR INSTALL_HDLDIR)

# CMake package version.
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
  VERSION ${SKBUILD_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
              ${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake
        DESTINATION ${INSTALL_CMAKEDIR})


# Testing
enable_testing()
add_subdirectory(tests EXCLUDE_FROM_ALL)
