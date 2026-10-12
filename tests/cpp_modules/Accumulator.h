#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

#include <cstdint>

/// A registered accumulator: q <= q + d on every rising clock edge, cleared while rst is high.
DSPSIM_MODULE(Accumulator)
{
public:
    dspsim::Input<uint8_t> clk{"clk"};
    dspsim::Input<uint8_t> rst{"rst"};
    dspsim::Input<int16_t> d{"d"};
    dspsim::Output<int32_t> q{"q"};

    DSPSIM_CTOR(Accumulator)
    {
        DSPSIM_CORO(eval);
    }

    dspsim::Task<> eval()
    {
        while (true)
        {
            co_await wait(clk.pos());
            q.write(rst.read() ? 0 : q.read() + d.read());
        }
    }
};
