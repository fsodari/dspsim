#include "dspsim/dspsim.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iostream>

using namespace dspsim;

TEST_CASE("Creating a Context", "[dspsim][context]")
{
    auto context = Context::context();
    auto same_context = Context::context();
    auto new_context = Context::create(12, 12);
    auto same_new_context = Context::context();

    std::cout << "Testing Context" << std::endl;
    std::cout << context->print_info() << std::endl;
    std::cout << same_context->print_info() << std::endl;
    std::cout << new_context->print_info() << std::endl;
    std::cout << same_new_context->print_info() << std::endl;
    std::cout << "Done Testing Context" << std::endl;
}
