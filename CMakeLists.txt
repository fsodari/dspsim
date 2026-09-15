cmake_minimum_required(VERSION 4.0)
project(${SKBUILD_PROJECT_NAME} VERSION ${SKBUILD_PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force release build.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Simplest way of exporting symbols for windows.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
# Windows requires the Python SABIModule component.
find_package(Python COMPONENTS Interpreter Development.Module Development.SABIModule REQUIRED)
message(STATUS "Python interpreter: ${Python_EXECUTABLE}")

# Find nanobind package.
find_package(nanobind CONFIG REQUIRED)
message(STATUS "Found nanobind: ${nanobind_DIR}")

# dspsim paths.
set(DSPSIM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DSPSIM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/framework/include)
set(DSPSIM_INSTALL_DIR ${SKBUILD_PROJECT_NAME}/framework)

# Install paths
include(GNUInstallDirs)
set(INSTALL_CMAKEDIR ${DSPSIM_INSTALL_DIR}/cmake)
set(INSTALL_LIBDIR ${DSPSIM_INSTALL_DIR})
set(INSTALL_BINDIR ${DSPSIM_INSTALL_DIR}/bin)
set(INSTALL_INCLUDEDIR ${DSPSIM_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})

# Set warning level
if (MSVC)
    # warning level 4
    add_compile_options(/W2)
else()
    # additional warnings
    add_compile_options(-Wall -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -Wno-maybe-uninitialized)
endif()

# Include dspsim utilities
include(${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake)

# Build dspsim core library
add_subdirectory(${DSPSIM_SRC_DIR})

# Build _framework module
nanobind_add_module(_framework
    FREE_THREADED
    NB_DOMAIN dspsim
    BACKEND_MODULE nanobind_backend
    NB_DOMAIN dspsim
    ${DSPSIM_SRC_DIR}/_framework.cpp)
target_link_libraries(_framework PRIVATE dspsim::dspsim-core)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${DSPSIM_INSTALL_DIR})

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
  PATH_VARS INSTALL_CMAKEDIR INSTALL_LIBDIR INSTALL_BINDIR INSTALL_INCLUDEDIR)

# CMake package version.
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
  VERSION ${SKBUILD_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
              ${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake
        DESTINATION ${INSTALL_CMAKEDIR})

# Install stubs. Call last so that all targets are installed when this is run.
dspsim_add_stub(_framework ${DSPSIM_INSTALL_DIR})
