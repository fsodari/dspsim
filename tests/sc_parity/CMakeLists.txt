add_custom_target(sc_parity_tests)
macro(add_sc_parity_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE dspsim::dspsim-core spdlog::spdlog SystemC::systemc)
    add_dependencies(sc_parity_tests ${name})

    add_test(NAME ${name} COMMAND ${name})
endmacro()

add_sc_parity_test(test_dff_parity)
