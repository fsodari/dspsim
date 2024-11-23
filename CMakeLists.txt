cmake_minimum_required(VERSION 3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_VERSION 20)
set(CMAKE_POSITION_INDEPENDENT_CODE On)

find_package(Python COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT} REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(verilator CONFIG REQUIRED HINTS $ENV{VERILATOR_ROOT})

message("DDIR: ${dspsim_DIR}")
find_package(dspsim CONFIG REQUIRED)

message("Found Python: ${Python_EXECUTABLE}")
message("Python Include: ${Python_INCLUDE_DIRS}")
message("Found Nanobind: ${nanobind_DIR}")
message("Found DSPSim: ${dspsim_DIR}")

# Add dspsim module
dspsim_add_module(_core src/main.cpp
    SOURCE ${DSPSIM_HDL_DIR}/SomeModel.sv)
install(TARGETS _core DESTINATION ${SKBUILD_PROJECT_NAME})
