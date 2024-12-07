#include "dspsim/clock.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iostream>
using namespace dspsim;
using namespace dspsim::units;

TEST_CASE("Creating a Clock", "[dspsim][clock]")
{
    std::cout << "Start Enter Context" << std::endl;
    {
        auto context = Context::create(1.0_ns, 1.0_ns);

        auto clk = Clock(10.0_ns);

        context->elaborate();

        std::cout << context->print_info() << std::endl;
        std::cout << "Done....." << std::endl;
    }
    std::cout << "Done Enter Context" << std::endl;
}