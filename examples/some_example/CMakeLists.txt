cmake_minimum_required(VERSION 3.30)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# Required packages.
find_package(Python COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT} REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(verilator CONFIG REQUIRED HINTS $ENV{VERILATOR_ROOT})
find_package(dspsim CONFIG REQUIRED)

message("Found Python: ${Python_EXECUTABLE}")
message("Python Include: ${Python_INCLUDE_DIRS}")
message("Found Nanobind: ${nanobind_DIR}")
message("Found Verilator: ${verilator_DIR}")
message("Found DSPSim: ${dspsim_DIR}")

# Include dspsim utilities
include(${dspsim_DIR}/dspsim-utils.cmake)

# Build a dspsim library module and generate stub files.
dspsim_add_module(_library
    CONFIG ${CMAKE_SOURCE_DIR}/pyproject.toml
    STUBS_DIR ${SKBUILD_PROJECT_NAME}
    INSTALL_DIR ${SKBUILD_PROJECT_NAME})

# Install extension
install(TARGETS _library
    LIBRARY DESTINATION ${SKBUILD_PROJECT_NAME})
