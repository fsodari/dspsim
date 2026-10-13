# The harness: counters, checksum, the measured loop and the command line. It uses neither engine.
add_library(bench-harness STATIC harness/bench.cpp harness/counters.cpp)
target_compile_options(bench-harness PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>)
target_include_directories(bench-harness PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# One executable per suite. scripts/benchmarks.py finds them through this list.
add_custom_target(benchmarks)
set(BENCHMARK_SUITES "")
macro(add_benchmark name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE bench-harness dspsim::dspsim-core SystemC::systemc)
    target_compile_options(${name} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>)
    add_dependencies(benchmarks ${name})
    list(APPEND BENCHMARK_SUITES ${name})
endmacro()

add_benchmark(chain)
add_benchmark(wide)
add_benchmark(events)
add_benchmark(sensitivity)
add_benchmark(timing)
add_benchmark(tasks)
add_benchmark(stream)
add_benchmark(structs)

# Verilated models: every module as a C++ model, for dspsim, and as the SystemC model that Verilator writes.
# They are one library, because verilate() adds Verilator's runtime to its target and a program can only have
# one copy of it. A target with a SystemC model compiles the runtime for SystemC (VM_SC), which the C++
# models work with as well.
if(verilator_FOUND)
    add_library(bench-vmodels STATIC)
    target_link_libraries(bench-vmodels PUBLIC dspsim::dspsim-core SystemC::systemc)
    # Verilator's headers are not written for the warnings that the benchmarks are compiled with.
    set_target_properties(bench-vmodels PROPERTIES SYSTEM TRUE)

    # Verilator asks for these when it writes a SystemC model. Nothing here uses its answer: the library
    # links the SystemC target.
    get_target_property(BENCH_SYSTEMC_INCLUDE SystemC::systemc INTERFACE_INCLUDE_DIRECTORIES)
    list(GET BENCH_SYSTEMC_INCLUDE 0 BENCH_SYSTEMC_INCLUDE)
    set(ENV{SYSTEMC_INCLUDE} "${BENCH_SYSTEMC_INCLUDE}")
    set(ENV{SYSTEMC_LIBDIR} "${BENCH_SYSTEMC_INCLUDE}/../lib")

    set(BENCH_HDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/hdl)
    foreach(module BusPorts BusPacked BusUnpacked)
        verilate(bench-vmodels
            SOURCES ${BENCH_HDL_DIR}/Bus.sv
            TOP_MODULE ${module}
            PREFIX V${module}
            VERILATOR_ARGS --quiet)
    endforeach()
    # Verilator has no SystemC model for a module with an unpacked struct port.
    foreach(module BusPorts BusPacked)
        verilate(bench-vmodels
            SOURCES ${BENCH_HDL_DIR}/Bus.sv
            TOP_MODULE ${module}
            PREFIX V${module}SC
            SYSTEMC
            VERILATOR_ARGS --quiet)
    endforeach()

    add_benchmark(verilated)
    target_link_libraries(verilated PRIVATE bench-vmodels)
endif()

# The suites, one name per line, for scripts/benchmarks.py.
list(JOIN BENCHMARK_SUITES "\n" BENCHMARK_SUITE_LINES)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/suites.txt "${BENCHMARK_SUITE_LINES}\n")
