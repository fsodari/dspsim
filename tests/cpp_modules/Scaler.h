#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

#include <cstddef>
#include <cstdint>

namespace demo
{
    /// The clock and reset ports that the modules below share.
    struct Clocked : public dspsim::Module
    {
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<uint8_t> rst{"rst"};

        Clocked(dspsim::ModuleName)
        {
        }
    };

    /// y[i] <= gain * x[i] on every rising clock edge.
    struct Scaler : public Clocked
    {
        dspsim::InputArray<int16_t> x{"x", {4}};
        dspsim::Input<double> gain{"gain"};
        dspsim::OutputArray<double> y{"y", {4}};

        Scaler(dspsim::ModuleName name) : Clocked(name)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());
                for (std::size_t i = 0; i < x.size(); ++i)
                {
                    y.flat(i).write(rst.read() ? 0.0 : gain.read() * x.flat(i).read());
                }
            }
        }
    };

    /// Only constructible with an extra argument: the builder cannot bind it yet.
    struct NeedsArgument : public dspsim::Module
    {
        dspsim::Input<uint8_t> clk{"clk"};

        NeedsArgument(dspsim::ModuleName, int depth) : depth(depth)
        {
        }

        int depth;
    };

    /// Not a module.
    struct NotAModule
    {
        int value = 0;
    };

    /// A class template: its parameters are not supported yet.
    template <typename T>
    struct Passthrough : public dspsim::Module
    {
        dspsim::Input<T> d{"d"};
        dspsim::Output<T> q{"q"};

        Passthrough(dspsim::ModuleName)
        {
        }
    };
} // namespace demo
