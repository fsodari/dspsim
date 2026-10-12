add_library(dspsim-core STATIC
    ${DSPSIM_SRC_DIR}/bitsel.cpp
    ${DSPSIM_SRC_DIR}/channel.cpp
    ${DSPSIM_SRC_DIR}/clock.cpp
    ${DSPSIM_SRC_DIR}/context.cpp
    ${DSPSIM_SRC_DIR}/engine_access.cpp
    ${DSPSIM_SRC_DIR}/coro.cpp
    ${DSPSIM_SRC_DIR}/derived_signal.cpp
    ${DSPSIM_SRC_DIR}/event.cpp
    ${DSPSIM_SRC_DIR}/log.cpp
    ${DSPSIM_SRC_DIR}/model.cpp
    ${DSPSIM_SRC_DIR}/module_name.cpp
    ${DSPSIM_SRC_DIR}/module.cpp
    ${DSPSIM_SRC_DIR}/ndarray.cpp
    ${DSPSIM_SRC_DIR}/port.cpp
    ${DSPSIM_SRC_DIR}/process.cpp
    # ${DSPSIM_SRC_DIR}/sensitivity_list.cpp
    ${DSPSIM_SRC_DIR}/signal.cpp
    ${DSPSIM_SRC_DIR}/struct.cpp)

add_library(dspsim::dspsim-core ALIAS dspsim-core)

# The core library is compiled as one translation unit (a unity build), so the compiler can inline across
# its source files. That replaces link-time optimization, which is not used: LTO puts compiler-specific
# intermediate code in the installed static library, and a project that links it would need the same
# compiler, in the same version, as the one that built the wheel.
# Turn the option off when working on the core itself: each source file is then compiled on its own, which
# rebuilds faster after a change to one of them, and the simulation runs slower.
option(DSPSIM_UNITY_BUILD "Compile dspsim-core as a single translation unit" ON)
set_target_properties(dspsim-core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    UNITY_BUILD ${DSPSIM_UNITY_BUILD}
    # 0 = all sources in one batch.
    UNITY_BUILD_BATCH_SIZE 0)
# log.cpp is the only source that includes spdlog, which takes far longer to compile than the rest of the
# core. Kept out of the unity batch, it is only recompiled when the logging code itself changes.
set_source_files_properties(${DSPSIM_SRC_DIR}/log.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

target_compile_features(dspsim-core PUBLIC cxx_std_23)

# GCC assumes that a function exported from a shared library may be replaced at load time (ELF symbol
# interposition), so in position independent code it does not inline such a function into its callers,
# not even within one source file. The Python extensions are shared libraries, and this flag is what lets
# GCC optimize the simulation run cycle in them. It is PUBLIC so that it also applies to the code that
# links the core: Verilator-generated models have the same problem. Clang never makes that assumption
# for inlining; the flag additionally lets it call such functions directly. Mach-O and PE targets have no
# interposition.
target_compile_options(dspsim-core PUBLIC
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<NOT:$<PLATFORM_ID:Darwin,Windows>>>:-fno-semantic-interposition>)

# Compile-time log level (see src/log.h): trace messages in debug builds only, debug messages in release builds.
target_compile_definitions(dspsim-core PRIVATE
    "$<$<CONFIG:Debug>:DSPSIM_LOG_ACTIVE_LEVEL=0>"
    "$<$<CONFIG:Release>:DSPSIM_LOG_ACTIVE_LEVEL=1>")

# Specific warnings as errors
# -Werror=reorder, /C5038. Modules must be initialized in same order as declaration.
target_compile_options(dspsim-core PRIVATE 
    # GCC and Clang syntax
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Werror=reorder
        -Wall
        -Werror
        -Wextra
        -Wpedantic
        -Wunused
        -Wunused-but-set-variable
        # No stack canaries in the engine. Some distributions (Debian, Ubuntu) build with
        # -fstack-protector-strong by default, which adds a canary check to every function that passes the
        # address of a local to a function that was not inlined. That includes Signal::write() and the
        # scheduler's stack pushes, whenever the compiler leaves the vector growth path out of line.
        -fno-stack-protector
        >
    
    # MSVC syntax: /weNNNN (where NNNN is the 4-digit warning number)
    # /bigobj: the unity translation unit can exceed the default limit on the number of object file sections.
    $<$<CXX_COMPILER_ID:MSVC>:
        /W2;/bigobj>
)

# Link to spdlog
target_link_libraries(dspsim-core PRIVATE $<BUILD_LOCAL_INTERFACE:spdlog::spdlog_header_only>)

# Include dirs
target_include_directories(dspsim-core PUBLIC
    $<BUILD_INTERFACE:${DSPSIM_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>)

# Public headers
target_sources(dspsim-core
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS ${DSPSIM_INCLUDE_DIR}
    FILES 
        ${DSPSIM_INCLUDE_DIR}/dspsim/bindings/task.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/bits.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/bitsel.h
${DSPSIM_INCLUDE_DIR}/dspsim/channel.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/clock.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/context.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/coro.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/detail/engine_access.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/derived_signal.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/dspsim.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/event.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/model.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/module_name.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/module.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/ndarray.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/port.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/process.h
        # ${DSPSIM_INCLUDE_DIR}/dspsim/sensitivity_list.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/signal.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/struct.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/modules/axis_rx.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/modules/axis_tx.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/modules/dff.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/flagged_stack.h
${DSPSIM_INCLUDE_DIR}/dspsim/utils/inlining.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/priority_queue.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/unique_stack.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/utils/utils.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/vmodule/vmodule.h
        ${DSPSIM_INCLUDE_DIR}/dspsim/vmodule/vport.h)
