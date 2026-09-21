#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

namespace dspsim
{
    template <typename T>
    class Dff : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<T> d{"d"};
        Output<T> q{"q"};

        Dff(ModuleName name);

        void some_process();
    };
} // namespace dspsim