find_package(SystemCLanguage REQUIRED)

macro(add_benchmark name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE dspsim::dspsim-core spdlog::spdlog SystemC::systemc)
endmacro()

add_benchmark(simple)
add_benchmark(nested_wire)
add_benchmark(nested_dff)
add_benchmark(nested_dff_really_deep)
add_benchmark(wide)
add_benchmark(heavy)
add_benchmark(nested_heavy)