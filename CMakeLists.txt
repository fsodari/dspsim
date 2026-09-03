cmake_minimum_required(VERSION 4.0)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
message(STATUS "Python interpreter: ${Python_EXECUTABLE}")
# message(STATUS "Python version: ${Python_VERSION}")
# message(STATUS "Python include dir: ${Python_INCLUDE_DIRS}")

find_package(nanobind CONFIG REQUIRED)
message(STATUS "Found nanobind: ${nanobind_DIR}")

# dspsim paths.
set(DSPSIM_PKG_DIR ${CMAKE_SOURCE_DIR}/src/dspsim)
set(DSPSIM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DSPSIM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/include)
set(DSPSIM_HDL_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/hdl)
set(DSPSIM_DATA_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/data)

# Install paths
include(GNUInstallDirs)
set(INSTALL_LIBDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_LIBDIR})
set(INSTALL_INCLUDEDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_INCLUDEDIR})
set(INSTALL_DATADIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_DATADIR})
set(INSTALL_HDLDIR ${SKBUILD_PROJECT_NAME}/hdl)
set(INSTALL_CMAKEDIR ${SKBUILD_PROJECT_NAME}/cmake)

# RPath
set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Include dspsim utilities
include(${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake)

# Build dspsim core library
add_subdirectory(${DSPSIM_SRC_DIR})

# Framework module
nanobind_add_module(_framework
    NB_DOMAIN dspsim
    BACKEND_MODULE nanobind_backend
    ${DSPSIM_SRC_DIR}/_framework.cpp
)
target_link_libraries(_framework PRIVATE dspsim::dspsim-core)
# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})

# Install stubs.
dspsim_add_stub(_framework ${SKBUILD_PROJECT_NAME})

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


# # Testing
# enable_testing()
# add_subdirectory(tests EXCLUDE_FROM_ALL)
