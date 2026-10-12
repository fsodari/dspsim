/*
    Sensitivity lists.

    fanin:  one process that is sensitive to n signals, which all change in the same delta cycle: n events
            for a single evaluation.
    anyof:  the same design when one of the n signals changes per clock cycle. The static kinds are
            sensitive to the n signals. The dynamic kinds wait for any of the n events instead: every
            evaluation subscribes the process to n events, and the event that occurs unsubscribes it from
            the other n - 1.
    tree:   a balanced tree of two-input adders, each a process that is sensitive to its two inputs. A change
            of the leaves settles in log2(n) delta cycles, with many evaluations in each.
    fanout: n processes that are sensitive to the same data signal: one event with n subscribers.

    Each design comes in several process kinds, which shows what a kind costs in each engine.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/sensitivity.h"
#include "models/sensitivity_sc.h"

#include <cstddef>
#include <cstdint>
#include <string>

using namespace benchmarks;

namespace
{
    using T = uint32_t;

    // Add one kind of the sum of n registers, of which `changes` change per clock cycle.
    template <template <typename> class Regs, template <typename> class Sum,
              template <typename> class RegsSC, template <typename> class SumSC>
    void add_fanin(Suite &suite, const std::string &group, const std::string &kind, size_t n, size_t changes,
                   Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Fanin<Regs<T>, Sum<T>, T>, T>>(n, changes);
        c.systemc<ClockedTbSC<FaninSC<RegsSC<T>, SumSC<T>, T>, T>>(n, changes);
    }

    // Add one kind of the adder tree with n leaves.
    template <template <typename> class Regs, template <typename> class Node,
              template <typename> class RegsSC, template <typename> class NodeSC>
    void add_tree(Suite &suite, const std::string &group, const std::string &kind, size_t n, Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Tree<Regs<T>, Node<T>, T>, T>>(n);
        c.systemc<ClockedTbSC<TreeSC<RegsSC<T>, NodeSC<T>, T>, T>>(n);
    }

    // Add one kind of the n taps on one signal.
    template <template <typename> class Tap, template <typename> class Sum,
              template <typename> class TapSC, template <typename> class SumSC>
    void add_fanout(Suite &suite, const std::string &group, const std::string &kind, size_t n, Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Fanout<Tap<T>, Sum<T>, T>, T>>(n);
        c.systemc<ClockedTbSC<FanoutSC<TapSC<T>, SumSC<T>, T>, T>>(n);
    }
}

int sc_main(int argc, char *argv[])
{
    Suite suite{"sensitivity"};

    {
        // All of the signals change on every clock edge.
        const std::string group = "fanin/n=64";
        const size_t n = 64;
        const Params params{.iters = 60'000};
        add_fanin<RegsMethod, SumMethod, RegsMethodSC, SumMethodSC>(suite, group, "method", n, n, params);
        add_fanin<RegsCoro, SumCoro, RegsThreadSC, SumThreadSC>(suite, group, "coro", n, n, params);
    }
    {
        // One of the signals changes on every clock edge. The static kinds are the baseline of the dynamic
        // ones: the difference is what it costs to subscribe to the events on every evaluation.
        const std::string group = "anyof/n=16";
        const size_t n = 16;
        const Params params{.iters = 300'000};
        add_fanin<RegsMethod, SumMethod, RegsMethodSC, SumMethodSC>(suite, group, "method", n, 1, params);
        add_fanin<RegsMethod, SumMethodDynamic, RegsMethodSC, SumMethodDynamicSC>(
            suite, group, "method_dynamic", n, 1, params);
        add_fanin<RegsCoro, SumCoro, RegsThreadSC, SumThreadSC>(suite, group, "coro", n, 1, params);
        add_fanin<RegsCoro, SumCoroDynamic, RegsThreadSC, SumThreadDynamicSC>(suite, group, "coro_dynamic", n, 1, params);
    }
    {
        // A longer list of events, where the subscriptions are most of the work.
        const std::string group = "anyof/n=64";
        const size_t n = 64;
        const Params params{.iters = 60'000};
        add_fanin<RegsMethod, SumMethodDynamic, RegsMethodSC, SumMethodDynamicSC>(
            suite, group, "method_dynamic", n, 1, params);
        add_fanin<RegsCoro, SumCoroDynamic, RegsThreadSC, SumThreadDynamicSC>(suite, group, "coro_dynamic", n, 1, params);
    }
    {
        // 255 adders in 8 levels.
        const Params params{.iters = 6'000};
        add_tree<RegsMethod, AddMethod, RegsMethodSC, AddMethodSC>(suite, "tree/n=256", "method", 256, params);
        add_tree<RegsCoro, AddCoro, RegsThreadSC, AddThreadSC>(suite, "tree/n=256", "coro", 256, params);
    }
    {
        const Params params{.iters = 8'000};
        add_fanout<TapMethod, SumMethod, TapMethodSC, SumMethodSC>(suite, "fanout/n=256", "method", 256, params);
        add_fanout<TapCoro, SumCoro, TapThreadSC, SumThreadSC>(suite, "fanout/n=256", "coro", 256, params);
    }
    return suite.main(argc, argv);
}
