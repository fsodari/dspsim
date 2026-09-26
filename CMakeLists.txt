cmake_minimum_required(VERSION 4.0)
project(${SKBUILD_PROJECT_NAME} VERSION ${SKBUILD_PROJECT_VERSION} LANGUAGES CXX)

# Set C++ standard to 23. 20?
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies installation directory
option(USE_LOCAL_DEPS "Add the deps/install folder to the module search path" ON)
option(INSTALL_DEV_DEPS "Install developer dependencies" OFF)
option(BUILD_BENCHMARKS "Build benchmark targets" ON)
option(BUILD_TESTS "Build test targets" ON)

# Install local dependencies if requested.
if(USE_LOCAL_DEPS)
    set(DEPS_INSTALL_DIR "${CMAKE_SOURCE_DIR}/deps/install")
    list(APPEND CMAKE_PREFIX_PATH "${DEPS_INSTALL_DIR}")

    # If it doesn't exist. Run the install script.
    if (NOT IS_DIRECTORY "${DEPS_INSTALL_DIR}")
        message(STATUS "Local dependencies directory not found: ${DEPS_INSTALL_DIR}")
        message(STATUS "Running install_deps.sh to set up local dependencies.")

        if (MSVC)
            set(SCRIPT_SUFFIX ".ps1")
        else()
            set(SCRIPT_SUFFIX ".sh")
        endif()

        # If installing dev dependencies, run the corresponding script. Otherwise, run the standard install script.
        if (${INSTALL_DEV_DEPS})
            execute_process(COMMAND ${CMAKE_SOURCE_DIR}/scripts/install_dev_deps${SCRIPT_SUFFIX})
        else()
            execute_process(COMMAND ${CMAKE_SOURCE_DIR}/scripts/install_deps${SCRIPT_SUFFIX}
            )
        endif()
    endif()
endif()

# Python required for nanobind/skbuild
find_package(Python COMPONENTS Interpreter Development.Module Development.SABIModule OPTIONAL)
message(STATUS "Python interpreter: ${Python_EXECUTABLE}")

# nanobind. Required to build _framework module. Optional for C++ projects.
find_package(nanobind REQUIRED)
message(STATUS "Found nanobind: ${nanobind_DIR}")

# spdlog. Use fetch content if not available
find_package(spdlog REQUIRED)
message(STATUS "spdlog: ${spdlog_DIR}")

# SystemC. Optional. Used for benchmarks.
find_package(SystemCLanguage OPTIONAL)
message(STATUS "SystemC: ${SystemCLanguage_DIR}")

# Catch2. Used for testing.
find_package(Catch2 3 OPTIONAL)
message(STATUS "Catch2: ${Catch2_DIR}")

# Verilator. Used by this project for testing, but it can be a runtime dependency for applications that use dspsim.
find_package(verilator OPTIONAL QUIET)
message(STATUS "Verilator: ${verilator_DIR}")

# Force all code to be position independent
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# dspsim paths.
set(DSPSIM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DSPSIM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/framework/include)

# Build core library
add_subdirectory(${DSPSIM_SRC_DIR})

### Installation configuration.
# Install paths
include(GNUInstallDirs)
set(DSPSIM_INSTALL_DIR ${SKBUILD_PROJECT_NAME}/framework)
set(INSTALL_CMAKEDIR ${DSPSIM_INSTALL_DIR}/cmake)
set(INSTALL_LIBDIR ${DSPSIM_INSTALL_DIR})
set(INSTALL_BINDIR ${DSPSIM_INSTALL_DIR}/bin)
set(INSTALL_INCLUDEDIR ${DSPSIM_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})

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

# Include dspsim utilities
include(${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake)

# Build _framework module
dspsim_add_nanobind_module(_framework ${DSPSIM_SRC_DIR}/_framework.cpp)
set_target_properties(_framework PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Install extension
install(TARGETS _framework
    LIBRARY DESTINATION ${DSPSIM_INSTALL_DIR})

# Install stubs. Call last so that all targets are installed when this is run.
dspsim_add_stub(_framework ${DSPSIM_INSTALL_DIR})

# Build benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks EXCLUDE_FROM_ALL)
endif()

# Testing.
if(BUILD_TESTS)
    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests/cpp EXCLUDE_FROM_ALL)
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests/sc_parity EXCLUDE_FROM_ALL)
endif()
