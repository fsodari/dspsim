#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include "eval_funcs.h"

namespace benchmarks
{

    template <typename M, typename T, int DEPTH>
    class NestedComb : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Signal<T> sig{"sig"};

        M some_model{"some_model"};
        NestedComb<M, T, DEPTH - 1> _nested{"nested"};

        NestedComb(dspsim::ModuleName name) : dspsim::Module(name)
        {
            some_model.clk.bind(clk);
            some_model.in.bind(in);
            some_model.out.bind(sig);

            _nested.clk.bind(clk);
            _nested.in.bind(sig);
            _nested.out.bind(out);
        }
    };

    template <typename M, typename T>
    class NestedComb<M, T, 0> : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        M some_model{"some_model"};

        NestedComb(dspsim::ModuleName name) : dspsim::Module(name)
        {
            some_model.clk.bind(clk);
            some_model.in.bind(in);
            some_model.out.bind(out);
        }
    };

    template <typename M, typename T, int DEPTH>
    SC_MODULE(NestedCombSC)
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_signal<T> sig{"sig"};

        M some_model{"some_model"};
        NestedCombSC<M, T, DEPTH - 1> _nested{"nested"};

        SC_CTOR(NestedCombSC)
        {
            some_model.clk.bind(clk);
            some_model.in.bind(in);
            some_model.out.bind(sig);

            _nested.clk.bind(clk);
            _nested.in.bind(sig);
            _nested.out.bind(out);
        }
    };

    template <typename M, typename T>
    class NestedCombSC<M, T, 0> : public sc_core::sc_module
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        M some_model{"some_model"};

        SC_CTOR(NestedCombSC)
        {
            some_model.clk.bind(clk);
            some_model.in.bind(in);
            some_model.out.bind(out);
        }
    };
}
