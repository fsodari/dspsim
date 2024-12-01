find_package(Catch2 REQUIRED)
add_executable(run_all_tests
    test_context.cpp
    test_model.cpp
    test_signal.cpp
    test_clock.cpp
    test_vmodel.cpp)
target_link_libraries(run_all_tests PRIVATE
    dspsim-core
    nanobind-static-abi3
    Catch2::Catch2WithMain)

verilate(run_all_tests TRACE
    SOURCES ${DSPSIM_HDL_DIR}/Foo.sv
)

include(CTest)
include(Catch)
catch_discover_tests(run_all_tests)