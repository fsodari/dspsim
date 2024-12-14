cmake_minimum_required(VERSION 3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
# set(CMAKE_POSITION_INDEPENDENT_CODE On)

# Required packages.
find_package(Python COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT} REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(VERILATOR CONFIG REQUIRED HINTS $ENV{VERILATOR_ROOT})

# include(FetchContent)
# FetchContent_Declare(
#   fmt
#   GIT_REPOSITORY https://github.com/fmtlib/fmt
#   GIT_TAG        e69e5f977d458f2650bb346dadf2ad30c5320281 # 10.2.1
#   EXCLUDE_FROM_ALL) 
  
# FetchContent_MakeAvailable(fmt)

message("Found Python: ${Python_EXECUTABLE}")
message("Python Include: ${Python_INCLUDE_DIRS}")
message("Found Nanobind: ${nanobind_DIR}")
message("Found Verilator: ${verilator_DIR}")
message("Found DSPSim: ${dspsim_DIR}")

include(GNUInstallDirs)

# DSPSim libraries.
set(DSPSIM_PKG_DIR ${CMAKE_SOURCE_DIR}/src/dspsim)
set(DSPSIM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DSPSIM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/include)
set(DSPSIM_HDL_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/hdl)

set(INSTALL_LIBDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_LIBDIR})
set(INSTALL_INCLUDEDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_INCLUDEDIR})
set(INSTALL_DATADIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_DATADIR})
set(INSTALL_CMAKEDIR ${SKBUILD_PROJECT_NAME}/${CMAKE_INSTALL_DATADIR}/cmake/dspsim)
set(INSTALL_HDLDIR ${SKBUILD_PROJECT_NAME}/hdl)

# Include dspsim utilities
include(${CMAKE_SOURCE_DIR}/cmake/dspsim-utils.cmake)

# dspsim-core, _framework, and _library modules.
add_subdirectory(src)

# Install core library targets.
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



# Package config.
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/dspsim-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
  INSTALL_DESTINATION ${INSTALL_CMAKEDIR}
  PATH_VARS INSTALL_CMAKEDIR INSTALL_LIBDIR INSTALL_INCLUDEDIR INSTALL_HDLDIR)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
  VERSION ${SKBUILD_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/dspsim-config-version.cmake
              ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dspsim-utils.cmake
        DESTINATION ${INSTALL_CMAKEDIR})
