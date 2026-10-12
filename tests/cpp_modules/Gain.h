#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

#include <cstdint>

namespace demo
{
    /// q <= (d * GAIN) >> SHIFT on every rising clock edge, negated if NEGATE is set. W is the type of q.
    template <typename T, int GAIN = 1, unsigned SHIFT = 0, bool NEGATE = false, typename W = int64_t>
    DSPSIM_MODULE(Gain)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> d{"d"};
        dspsim::Output<W> q{"q"};

        DSPSIM_CTOR(Gain)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());
                const W value = (static_cast<W>(d.read()) * GAIN) >> SHIFT;
                q.write(NEGATE ? -value : value);
            }
        }
    };

    /// The defaults of WIDTH and Q depend on the parameters before them.
    template <typename T, int WIDTH = sizeof(T) * 8, typename Q = T, bool REGISTERED = false>
    struct Resize : public dspsim::Module
    {
        dspsim::Input<T> d{"d", WIDTH};
        dspsim::Output<Q> q{"q"};

        Resize(dspsim::ModuleName)
        {
        }
    };

    /// A type that numpy does not have.
    struct Sample
    {
        int16_t i = 0;
        int16_t q = 0;

        bool operator==(const Sample &) const = default;
    };

    /// Ports that depend on a template parameter, for a base class.
    template <typename T>
    struct Stream : public dspsim::Module
    {
        dspsim::Input<T> tdata{"tdata"};
        dspsim::Input<uint8_t> tvalid{"tvalid"};

        Stream(dspsim::ModuleName)
        {
        }
    };

    /// A class that derives from an instance of a class template.
    struct ByteSink : public Stream<uint8_t>
    {
        dspsim::Output<uint8_t> tready{"tready"};

        ByteSink(dspsim::ModuleName name) : Stream<uint8_t>(name)
        {
        }
    };

    /// A class template whose base class depends on its parameter.
    template <typename T>
    struct Sink : public Stream<T>
    {
        dspsim::Output<uint8_t> tready{"tready"};

        Sink(dspsim::ModuleName name) : Stream<T>(name)
        {
        }
    };
} // namespace demo
