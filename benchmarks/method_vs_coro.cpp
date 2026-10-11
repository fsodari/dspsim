/*
    Compares the process kinds on the same model: a chain of DFFs clocked by one clock.

    - method: a method process with static sensitivity (always(clk.pos())). SystemC: SC_METHOD.
    - method_dynamic: a method process with dynamic sensitivity (next_trigger(clk.pos()) on every evaluation).
      SystemC: SC_METHOD with next_trigger(clk.posedge_event()).
    - coro: a coroutine process with dynamic sensitivity (co_await wait(clk.pos())), which subscribes to the
      event again on every wait. SystemC: SC_THREAD with wait(clk.posedge_event()).
    - coro_static: a coroutine process with static sensitivity (co_await wait()), to separate the cost of the
      coroutine from the cost of the dynamic subscription. SystemC: SC_THREAD with wait().

    Each kind runs on both engines, and all of the runs produce the same checksum.
*/
#include <dspsim/dspsim.h>
#include <systemc>
#include "utils.h"
#include "nested.h"
#include "simple.h"

using namespace benchmarks;
using namespace dspsim;

namespace
{
    // Method process, static sensitivity.
    template <typename T>
    DSPSIM_MODULE(MethodDff)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(MethodDff)
        {
            // The coroutines only write after a clock edge, so skip the initial evaluation.
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    // Method process, dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(DynamicMethodDff)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(DynamicMethodDff)
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

    // Coroutine process, dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(DynamicCoroDff)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(DynamicCoroDff)
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

    // SystemC equivalent of MethodDff: SC_METHOD, static sensitivity.
    template <typename T>
    SC_MODULE(MethodDffSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(MethodDffSC)
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

    // SystemC equivalent of DynamicMethodDff: SC_METHOD, dynamic sensitivity.
    template <typename T>
    SC_MODULE(DynamicMethodDffSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(DynamicMethodDffSC)
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

    // SystemC equivalent of DynamicCoroDff: SC_THREAD, dynamic sensitivity.
    template <typename T>
    SC_MODULE(DynamicThreadDffSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(DynamicThreadDffSC)
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

    // SystemC equivalent of SimpleDff: SC_THREAD, static sensitivity.
    template <typename T>
    SC_MODULE(StaticThreadDffSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(StaticThreadDffSC)
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

    struct Result
    {
        double dspsim = 0;
        double systemc = 0;
    };

    /*
        Run one process kind on both engines. SystemC elaborates once per simulation context, so each
        SystemC run gets a fresh one.
    */
    template <typename M, typename MSC, typename T>
    Result run(const std::string &label, const std::string &kind, const std::string &engine, size_t n_iter, int run_step)
    {
        Result result;
        if (kind != label && kind != "all")
        {
            return result;
        }
        if (engine == "dspsim" || engine == "both")
        {
            std::cout << std::left << std::setw(14) << label << ": ";
            result.dspsim = dspsim_runner<M, T>(n_iter, run_step);
        }
        if (engine == "systemc" || engine == "both")
        {
            sc_core::sc_curr_simcontext = new sc_core::sc_simcontext();
            sc_core::sc_default_global_context = sc_core::sc_curr_simcontext;
            std::cout << std::left << std::setw(14) << label << ": ";
            result.systemc = systemc_runner<MSC, T>(n_iter, run_step);
        }
        return result;
    }
} // namespace

int sc_main(int argc, char *argv[])
{
    using T = uint32_t;
    size_t n_iter = argc > 1 ? std::stoul(argv[1]) : 1000000;
    size_t run_step = argc > 2 ? std::stoul(argv[2]) : 10;
    // Optional 3rd arg to isolate one process kind for profiling: "method", "method_dynamic", "coro",
    // "coro_static", or "all" (default).
    std::string kind = argc > 3 ? argv[3] : "all";
    // Optional 4th arg to isolate one engine: "dspsim", "systemc", or "both" (default).
    std::string engine = argc > 4 ? argv[4] : "both";

    constexpr size_t depth = 500;

    auto method = run<NestedComb<MethodDff<T>, T, depth>, NestedCombSC<MethodDffSC<T>, T, depth>, T>(
        "method", kind, engine, n_iter, run_step);
    auto method_dynamic = run<NestedComb<DynamicMethodDff<T>, T, depth>, NestedCombSC<DynamicMethodDffSC<T>, T, depth>, T>(
        "method_dynamic", kind, engine, n_iter, run_step);
    auto coro = run<NestedComb<DynamicCoroDff<T>, T, depth>, NestedCombSC<DynamicThreadDffSC<T>, T, depth>, T>(
        "coro", kind, engine, n_iter, run_step);
    // SimpleDff is the coroutine with static sensitivity.
    auto coro_static = run<NestedComb<SimpleDff<T>, T, depth>, NestedCombSC<StaticThreadDffSC<T>, T, depth>, T>(
        "coro_static", kind, engine, n_iter, run_step);

    const std::vector<std::pair<std::string, Result>> results = {
        {"method", method}, {"method_dynamic", method_dynamic}, {"coro", coro}, {"coro_static", coro_static}};

    std::cout << std::fixed << std::setprecision(2);
    if (kind == "all")
    {
        // Each kind relative to the method with static sensitivity, per engine.
        for (const auto &[label, result] : results)
        {
            if (label == "method")
            {
                continue;
            }
            std::cout << std::left << std::setw(14) << label << " / method:";
            if (engine != "systemc")
            {
                std::cout << " DSPSim " << result.dspsim / method.dspsim << "x";
            }
            if (engine != "dspsim")
            {
                std::cout << " SystemC " << result.systemc / method.systemc << "x";
            }
            std::cout << "\n";
        }
    }
    if (engine == "both")
    {
        for (const auto &[label, result] : results)
        {
            if (kind == label || kind == "all")
            {
                std::cout << "DSPSim / SystemC " << std::left << std::setw(14) << label << ": " << result.dspsim / result.systemc << "x\n";
            }
        }
    }
    return 0;
}
