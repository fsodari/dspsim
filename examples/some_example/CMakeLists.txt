cmake_minimum_required(VERSION 4.0)
project(some_example)

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

# # Find nanobind package.
# find_package(nanobind CONFIG REQUIRED)
# message(STATUS "Found nanobind: ${nanobind_DIR}")

# # Find verilator
# find_package(verilator REQUIRED)
# message(STATUS "Found verilator: ${verilator_DIR}")

find_package(dspsim REQUIRED)
message(STATUS "Found dspsim: ${dspsim_DIR}")

dspsim_generate("pyproject.toml" ${CMAKE_SOURCE_DIR})
