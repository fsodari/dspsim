#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <deque>

namespace dspsim
{
    //
    template <typename T>
    class AxisRx : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        Input<T> s_axis_tdata{"s_axis_tdata"};
        Input<uint8_t> s_axis_tvalid{"s_axis_tvalid"};
        Output<uint8_t> s_axis_tready{"s_axis_tready"};

        std::deque<T> fifo;
        uint8_t _ready = 0;

        AxisRx(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(&AxisRx<T>::eval_);
            sensitive << clk.pos();
        }

        void eval_()
        {
            if (clk.posedge())
            {
                if (s_axis_tvalid.read() && s_axis_tready.read())
                {
                    fifo.push_back(s_axis_tdata.read());
                }

                // No limit to buffer size or throughput, so set ready when user requests.
                s_axis_tready.write(_ready);
            }
        }

        void ready(uint8_t r)
        {
            _ready = r;
        }
        uint8_t ready() const
        {
            return _ready;
        }
    };
}