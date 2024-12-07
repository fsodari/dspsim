#include "dspsim/model.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iostream>
using namespace dspsim;

class SomeModel : public Model
{
public:
    int x = 0;
    SomeModel(int x) : x(x)
    {
    }

    void eval_step()
    {
    }
    void eval_end_step()
    {
    }
};

TEST_CASE("Creating a Model", "[dspsim][model]")
{
    std::cout << "Start Enter Context" << std::endl;
    {
        auto context = Context::create();

        auto some_model = SomeModel(42);
        auto another = SomeModel(23);
        context->elaborate();

        std::cout << context->print_info() << std::endl;

        REQUIRE(some_model.x == 42);
        REQUIRE(another.x == 23);
        std::cout << "Done....." << std::endl;
    }
    std::cout << "Done Enter Context" << std::endl;
}

TEST_CASE("Creating a Model 2", "[dspsim][model]")
{
    std::cout << "Start Enter Context" << std::endl;
    {
        auto context = Context::create();
        auto some_model = SomeModel(42);
        auto another = SomeModel(23);

        context->elaborate();

        std::cout << context->print_info() << std::endl;

        REQUIRE(some_model.x == 42);
        REQUIRE(another.x == 23);
        std::cout << "Done....." << std::endl;
    }
    std::cout << "Done Enter Context" << std::endl;
}