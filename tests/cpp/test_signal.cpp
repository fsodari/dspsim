#include "dspsim/signal.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iostream>
#include <array>

using namespace dspsim;
using namespace dspsim::units;

TEST_CASE("Creating a Signal", "[dspsim][signal]")
{
    const static std::array<std::array<int, 3>, 2> x{{{1, 2, 3}, {4, 5, 6}}};

    std::cout << "Start Enter Context" << std::endl;
    {
        auto context = Context::create(10.0_ns, 5.0_ns);

        auto a = Signal<uint8_t>();

        auto arr = Signal<uint8_t>::new_array(4);

        context->elaborate();

        std::cout << context->print_info() << std::endl;
        std::cout << "Done....." << std::endl;
    }
    std::cout << "Done Enter Context" << std::endl;
}