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

# The suites, one name per line, for scripts/benchmarks.py.
list(JOIN BENCHMARK_SUITES "\n" BENCHMARK_SUITE_LINES)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/suites.txt "${BENCHMARK_SUITE_LINES}\n")
