#include "dspsim/dspsim.h"
#include "dspsim/vmodel.h"
#include "dspsim/port.h"

#include "VFoo.h"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iostream>

using namespace dspsim;
using namespace dspsim::units;

class Foo : public VModel<VFoo, VerilatedVcdC>
{
    Input<ptype(top->clk)> clk;
    Input<ptype(top->rst)> rst;
    Input<ptype(top->x)> x;
    Output<ptype(top->y)> y;

public:
    Foo(Signal<uint8_t> &clk, Signal<uint8_t> &rst, Signal<uint32_t> &x, Signal<uint32_t> &y)
        : clk(clk, top->clk), rst(rst, top->rst), x(x, top->x), y(y, top->y)
    {
    }

    // Foo(Signal<uint8_t> *clk, Signal<uint8_t> *rst, Signal<uint32_t> *x, Signal<uint32_t> *y)
    //     : clk(clk, top->clk), rst(rst, top->rst), x(x, top->x), y(y, top->y)
    // {
    // }
    // Foo(std::weak_ptr<Signal<uint8_t>> clk,
    //     std::weak_ptr<Signal<uint8_t>> rst,
    //     std::weak_ptr<Signal<uint32_t>> x,
    //     std::weak_ptr<Signal<uint32_t>> y)
    //     : clk(clk, top->clk), rst(rst, top->rst), x(x, top->x), y(y, top->y)
    // {
    // }
};

TEST_CASE("Creating a VModel", "[dspsim][vmodel]")
{
    std::cout << "Start Enter Context" << std::endl;
    {
        auto sim = Context::create(1.0_ns, 1.0_ns);

        auto clk = Clock(10.0_ns);
        auto rst = Dff<uint8_t>(clk, 1);
        auto x = Signal<uint32_t>(0);
        auto y = Signal<uint32_t>();

        auto foo = Foo(clk, rst, x, y);

        foo.trace("foo.vcd");

        sim->elaborate();

        rst = 1;
        sim->advance(100);
        rst = 0;
        sim->advance(100);

        for (int i = 42; i < 50; i++)
        {
            x = i;
            sim->advance(10);
        }

        std::cout << sim->print_info() << std::endl;
        std::cout << "Done....." << std::endl;
    }
    std::cout << "Done Enter Context" << std::endl;
}