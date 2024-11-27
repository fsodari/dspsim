cmake_minimum_required(VERSION 3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_VERSION 20)
# Doing this makes shared libaries work.
set(CMAKE_INSTALL_RPATH $ORIGIN/lib)

# Required packages.
find_package(Python COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT} REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(verilator CONFIG REQUIRED HINTS $ENV{VERILATOR_ROOT})

message("Found Python: ${Python_EXECUTABLE}")
message("Python Include: ${Python_INCLUDE_DIRS}")
message("Found Nanobind: ${nanobind_DIR}")
message("Found Verilator: ${verilator_DIR}")
message("Found DSPSim: ${dspsim_DIR}")

# DSPSim libraries.
add_subdirectory(src)

# Package config.

set(PKG_INSTALL_DIR ${SKBUILD_PROJECT_NAME})
set(INCLUDE_INSTALL_DIR ${SKBUILD_PROJECT_NAME}/include)
set(HDL_INSTALL_DIR ${SKBUILD_PROJECT_NAME}/hdl)

include(CMakePackageConfigHelpers)
configure_package_config_file(src/cmake/dspsim-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
  INSTALL_DESTINATION ${SKBUILD_PROJECT_NAME}/cmake
  PATH_VARS PKG_INSTALL_DIR INCLUDE_INSTALL_DIR HDL_INSTALL_DIR)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
  VERSION ${SKBUILD_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
              ${CMAKE_CURRENT_SOURCE_DIR}/src/cmake/dspsim-utils.cmake
        DESTINATION ${SKBUILD_PROJECT_NAME}/cmake)
