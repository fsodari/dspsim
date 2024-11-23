# cmake_minimum_required(VERSION 3.26)
# project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

# set(CMAKE_CXX_VERSION 20)
# set(CMAKE_POSITION_INDEPENDENT_CODE On)

# find_package(Python COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT} REQUIRED)
# find_package(nanobind CONFIG REQUIRED)
# message("Verilator Root: $ENV{VERILATOR_ROOT}")
# find_package(verilator CONFIG REQUIRED HINTS $ENV{VERILATOR_ROOT})

# # message("DDIR: ${dspsim_DIR}")
# find_package(dspsim CONFIG REQUIRED)

# message("Found Python: ${Python_EXECUTABLE}")
# message("Python Include: ${Python_INCLUDE_DIRS}")
# message("Found Nanobind: ${nanobind_DIR}")
# message("Found DSPSim: ${dspsim_DIR}")

# # # Set the dspsim environment
# # dspsim_set_environment()
# Add dspsim module
nanobind_add_module(_framework ${CMAKE_CURRENT_SOURCE_DIR}/src/_framework.cpp STABLE_ABI)
dspsim_build_library(dspsim_lib)
target_link_libraries(_framework PRIVATE dspsim_lib)

install(TARGETS _framework DESTINATION ${SKBUILD_PROJECT_NAME})
