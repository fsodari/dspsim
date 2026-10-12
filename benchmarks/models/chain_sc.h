/*
    SystemC counterparts of chain.h. A method process is an SC_METHOD and a coroutine process an SC_THREAD.
*/
#pragma once
#include "level_storage.h"

#include <systemc>

namespace benchmarks
{
    /// SC_METHOD with static sensitivity.
    template <typename T>
    SC_MODULE(DffMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(DffMethodSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    /// SC_METHOD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(DffMethodDynamicSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(DffMethodDynamicSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            out.write(in.read());
            next_trigger(clk.posedge_event());
        }
    };

    /// SC_THREAD with static sensitivity.
    template <typename T>
    SC_MODULE(DffThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(DffThreadSC)
        {
            SC_THREAD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            while (true)
            {
                wait();
                out.write(in.read());
            }
        }
    };

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(DffThreadDynamicSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(DffThreadDynamicSC)
        {
            SC_THREAD(eval);
        }

        void eval()
        {
            while (true)
            {
                wait(clk.posedge_event());
                out.write(in.read());
            }
        }
    };

    template <typename T>
    SC_MODULE(WireMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WireMethodSC)
        {
            SC_METHOD(eval);
            sensitive << in;
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    template <typename T>
    SC_MODULE(WireMethodDynamicSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WireMethodDynamicSC)
        {
            SC_METHOD(eval);
        }

        void eval()
        {
            out.write(in.read());
            next_trigger(in.value_changed_event());
        }
    };

    template <typename T>
    SC_MODULE(WireThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WireThreadSC)
        {
            SC_THREAD(eval);
            sensitive << in;
        }

        void eval()
        {
            while (true)
            {
                out.write(in.read());
                wait();
            }
        }
    };

    template <typename T>
    SC_MODULE(WireThreadDynamicSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WireThreadDynamicSC)
        {
            SC_THREAD(eval);
        }

        void eval()
        {
            while (true)
            {
                out.write(in.read());
                wait(in.value_changed_event());
            }
        }
    };

    /// One level of a ChainSC. See ChainLevel in chain.h.
    template <typename M, typename T>
    class ChainLevelSC : public sc_core::sc_module
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_signal<T> sig{"sig"};

        M stage{"stage"};
        ChainLevelSC *nested = nullptr;

        ChainLevelSC(sc_core::sc_module_name name, int more, LevelStorage<ChainLevelSC> &storage)
            : sc_core::sc_module(name)
        {
            stage.clk.bind(clk);
            stage.in.bind(in);
            if (more > 0)
            {
                nested = new (storage.next()) ChainLevelSC("nested", more - 1, storage);
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
        ~ChainLevelSC() override
        {
            if (nested)
            {
                nested->~ChainLevelSC();
            }
        }
    };

    /// `stages` stages M in series. See Chain in chain.h.
    template <typename M, typename T>
    class ChainSC : private LevelStorage<ChainLevelSC<M, T>>, public ChainLevelSC<M, T>
    {
    public:
        ChainSC(sc_core::sc_module_name name, int stages)
            : LevelStorage<ChainLevelSC<M, T>>(stages - 1), ChainLevelSC<M, T>(name, stages - 1, *this) {}
    };
}
