cmake_minimum_required(VERSION 4.0)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

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

# Find verilator
find_package(verilator REQUIRED)
message(STATUS "Found verilator: ${verilator_DIR}")

# dspsim paths.
set(DSPSIM_PKG_DIR ${CMAKE_SOURCE_DIR}/src/dspsim)
set(DSPSIM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DSPSIM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/include)
set(DSPSIM_HDL_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/hdl)
set(DSPSIM_DATA_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/data)

# Install paths
include(GNUInstallDirs)
set(INSTALL_LIBDIR ${SKBUILD_PROJECT_NAME})
set(INSTALL_BINDIR ${SKBUILD_PROJECT_NAME})
set(INSTALL_INCLUDEDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_INCLUDEDIR})
set(INSTALL_DATADIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_DATADIR})
set(INSTALL_HDLDIR ${SKBUILD_PROJECT_NAME}/hdl)
set(INSTALL_CMAKEDIR ${SKBUILD_PROJECT_NAME}/cmake)

# RPath
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Set warning level
if (MSVC)
    # warning level 4
    add_compile_options(/W4)
else()
    # additional warnings
    add_compile_options(-Wall -Wno-sign-compare -Wno-unused-parameter -Wno-unused-variable -Wno-maybe-uninitialized)
endif()

# Include dspsim utilities
include(${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake)

# message(FATAL_ERROR "Python Path: $ENV{PYTHONPATH}")
# Build dspsim core library
add_subdirectory(${DSPSIM_SRC_DIR})

# Build _framework module
nanobind_add_module(_framework
    NB_DOMAIN dspsim
    BACKEND_MODULE nanobind_backend
    NB_DOMAIN dspsim
    ${DSPSIM_SRC_DIR}/_framework.cpp)
target_link_libraries(_framework PRIVATE dspsim::dspsim-core)

# Verilated models
verilate(_framework
    SOURCES ${DSPSIM_HDL_DIR}/SimpleModel.sv
    # TRACE_VCD
    TRACE_FST
    VERILATOR_ARGS --quiet)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# ### Library Module ###
# # Create library module using dspsim utilities.
# dspsim_add_module(_library
#     CONFIG ${CMAKE_SOURCE_DIR}/pyproject.toml
#     STUBS_DIR ${SKBUILD_PROJECT_NAME}
#     INSTALL_DIR ${SKBUILD_PROJECT_NAME})


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
  PATH_VARS INSTALL_CMAKEDIR INSTALL_LIBDIR INSTALL_BINDIR INSTALL_INCLUDEDIR INSTALL_HDLDIR)

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
dspsim_add_stub(_framework ${SKBUILD_PROJECT_NAME})

########## Build Library Module ##########

# Update PYTHONPATH in environment so the generate command can be run
set(ENV{PYTHONPATH} "${CMAKE_SOURCE_DIR}/src/")

# # Install dspsim_generate module
# dspsim_generate(
#     ${CMAKE_SOURCE_DIR}/pyproject.toml
#     ${CMAKE_BINARY_DIR})