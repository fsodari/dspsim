#pragma once
#include <dspsim/module.h>
#include <dspsim/clock.h>

namespace dspsim
{
    template <typename T>
    class Dff : public Module
    {
    public:
        Input<uint8_t> clk;
        Input<T> d;
        Output<T> q;

        Dff(ModuleName name, Signal<uint8_t> &clk_, Signal<T> &d_, Signal<T> &q_);

        void eval() override;
    };
} // namespace dspsim