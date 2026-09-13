cmake_minimum_required(VERSION 4.0)
project(${SKBUILD_PROJECT_NAME})

# Force release build.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

find_package(dspsim REQUIRED)
message(STATUS "Found dspsim: ${dspsim_DIR}")

dspsim_add_module(library ${CMAKE_SOURCE_DIR}/pyproject.toml ${SKBUILD_PROJECT_NAME})
