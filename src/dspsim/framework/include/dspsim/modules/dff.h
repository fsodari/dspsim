#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

namespace dspsim
{
    template <typename T>
    DSPSIM_MODULE(Dff)
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<T> d{"d"};
        Output<T> q{"q"};

        DSPSIM_CTOR(Dff)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
        }

        void eval()
        {
            if (clk.posedge())
            {
                q.write(d.read());
            }
        }
    };
} // namespace dspsim