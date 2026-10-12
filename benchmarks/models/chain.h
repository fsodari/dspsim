/*
    dspsim models for the chain benchmarks: a stage with a clock, an input and an output, in each process
    kind, and Chain, which connects any number of stages in series. chain_sc.h has the SystemC counterparts.
*/
#pragma once
#include "level_storage.h"

#include <dspsim/dspsim.h>

#include <cstdint>

namespace benchmarks
{
    /*
        A D flip-flop, once per process kind. The four produce the same output.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(DffMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(DffMethod)
        {
            // The coroutine kinds only write after a clock edge, so skip the initial evaluation.
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    /// Method process with dynamic sensitivity: next_trigger() on every evaluation.
    template <typename T>
    DSPSIM_MODULE(DffMethodDynamic)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(DffMethodDynamic)
        {
            // Static sensitivity only triggers the first evaluation. After that, next_trigger() overrides it.
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            out.write(in.read());
            next_trigger(clk.pos());
        }
    };

    /// Coroutine process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(DffCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(DffCoro)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait();
                out.write(in.read());
            }
        }
    };

    /// Coroutine process with dynamic sensitivity: it subscribes to the event again on every wait.
    template <typename T>
    DSPSIM_MODULE(DffCoroDynamic)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(DffCoroDynamic)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());
                out.write(in.read());
            }
        }
    };

    /*
        A wire (out = in), once per process kind. The clock is not used: the stage is combinational.
    */
    template <typename T>
    DSPSIM_MODULE(WireMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WireMethod)
        {
            DSPSIM_METHOD(eval)
                ->always(in);
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    template <typename T>
    DSPSIM_MODULE(WireMethodDynamic)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WireMethodDynamic)
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            out.write(in.read());
            next_trigger(in.change());
        }
    };

    template <typename T>
    DSPSIM_MODULE(WireCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WireCoro)
        {
            DSPSIM_CORO(eval)
                ->always(in);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                out.write(in.read());
                co_await wait();
            }
        }
    };

    template <typename T>
    DSPSIM_MODULE(WireCoroDynamic)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WireCoroDynamic)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                out.write(in.read());
                co_await wait(in.change());
            }
        }
    };

    /*
        One level of a Chain: a stage followed by the rest of the chain, nested inside it.

            in -> stage -> sig -> nested -> out
            in -> stage -> out                     (innermost level, which leaves sig unused)
    */
    template <typename M, typename T>
    DSPSIM_MODULE(ChainLevel)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Signal<T> sig{"sig"};

        M stage{"stage"};
        ChainLevel *nested = nullptr;

        /// `more` is the number of levels nested inside this one.
        ChainLevel(::dspsim::ModuleName, int more, LevelStorage<ChainLevel> &storage)
        {
            stage.clk.bind(clk);
            stage.in.bind(in);
            if (more > 0)
            {
                nested = new (storage.next()) ChainLevel("nested", more - 1, storage);
                stage.out.bind(sig);

                nested->clk.bind(clk);
                nested->in.bind(sig);
                nested->out.bind(out);
            }
            else
            {
                stage.out.bind(out);
            }
        }
        ChainLevel(const ChainLevel &) = delete;
        ChainLevel &operator=(const ChainLevel &) = delete;
        ~ChainLevel()
        {
            if (nested)
            {
                nested->~ChainLevel();
            }
        }
    };

    /// `stages` stages M in series, each nested inside the one before it. See LevelStorage for the layout.
    template <typename M, typename T>
    struct Chain : private LevelStorage<ChainLevel<M, T>>, public ChainLevel<M, T>
    {
        Chain(::dspsim::ModuleName name, int stages)
            : LevelStorage<ChainLevel<M, T>>(stages - 1), ChainLevel<M, T>(name, stages - 1, *this) {}
    };
}
