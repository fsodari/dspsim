/*
    Events that a model notifies itself: SensitivityEvent::notify() in dspsim, a delta notification of an
    sc_event in SystemC. The chain and wide suites only use the events of signals.

    ring:      a token passes through n nodes, each waking the next. One notification and one evaluation
               per delta cycle.
    broadcast: one event with n subscribers. One notification makes n processes runnable.
    handshake: initiators and targets exchange requests and responses through a pair of events, several
               round trips per clock cycle, in n pairs at once.
    fifo:      producers and consumers share bounded queues that notify on every push and pop, and block
               when the queue is full or empty, in n pairs at once.

    Each design comes in several process kinds, which shows what it costs to be statically sensitive to
    such an event and to wait on it dynamically.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/chain.h"
#include "models/chain_sc.h"
#include "models/events.h"
#include "models/events_sc.h"

#include <string>

using namespace benchmarks;

namespace
{
    using T = uint32_t;

    // Add one kind of a design Top with n parts of type Part: a ring of nodes, a broadcaster with listeners.
    template <template <typename, typename> class Top, template <typename, typename> class TopSC,
              template <typename> class Part, template <typename> class PartSC>
    void add_parts(Suite &suite, const std::string &group, const std::string &kind, int n, Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Top<Part<T>, T>, T>>(n);
        c.systemc<ClockedTbSC<TopSC<PartSC<T>, T>, T>>(n);
    }

    // Add one kind of a chain of `stages` stages, each a pair of modules: Stage and StageSC are the stages.
    template <typename Stage, typename StageSC>
    void add_pairs(Suite &suite, const std::string &group, const std::string &kind, int stages, Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Chain<Stage, T>, T>>(stages);
        c.systemc<ClockedTbSC<ChainSC<StageSC, T>, T>>(stages);
    }
}

int sc_main(int argc, char *argv[])
{
    Suite suite{"events"};

    {
        constexpr int n = 64;
        const std::string group = "ring/n=64";
        const Params params{.iters = 40'000};
        add_parts<Ring, RingSC, RingNodeMethod, RingNodeMethodSC>(suite, group, "method", n, params);
        add_parts<Ring, RingSC, RingNodeMethodDynamic, RingNodeMethodDynamicSC>(
            suite, group, "method_dynamic", n, params);
        add_parts<Ring, RingSC, RingNodeCoro, RingNodeThreadSC>(suite, group, "coro", n, params);
        add_parts<Ring, RingSC, RingNodeCoroDynamic, RingNodeThreadDynamicSC>(suite, group, "coro_dynamic", n, params);
    }
    {
        constexpr int n = 500;
        const std::string group = "broadcast/n=500";
        const Params params{.iters = 12'000};
        add_parts<Broadcast, BroadcastSC, ListenerMethod, ListenerMethodSC>(suite, group, "method", n, params);
        add_parts<Broadcast, BroadcastSC, ListenerMethodDynamic, ListenerMethodDynamicSC>(
            suite, group, "method_dynamic", n, params);
        add_parts<Broadcast, BroadcastSC, ListenerCoro, ListenerThreadSC>(suite, group, "coro", n, params);
        add_parts<Broadcast, BroadcastSC, ListenerCoroDynamic, ListenerThreadDynamicSC>(
            suite, group, "coro_dynamic", n, params);
    }
    {
        // 16 pairs, each making 8 round trips per clock cycle. The warm-up has to fill the chain.
        constexpr int n = 16;
        constexpr int trips = 8;
        const std::string group = "handshake/n=16,trips=8";
        const Params params{.iters = 15'000};
        add_pairs<Handshake<InitiatorMethod, TargetMethod, T, trips>,
                  HandshakeSC<InitiatorMethodSC, TargetMethodSC, T, trips>>(
            suite, group, "method", n, params);
        add_pairs<Handshake<InitiatorCoroDynamic, TargetCoroDynamic, T, trips>,
                  HandshakeSC<InitiatorThreadDynamicSC, TargetThreadDynamicSC, T, trips>>(
            suite, group, "coro_dynamic", n, params);
    }
    {
        /*
            16 pairs, each moving a burst of 64 items per clock cycle through a fifo that holds 16, the
            default size of an sc_fifo: every item is two notifications, and the producer and the consumer
            are each woken four times per burst.

            coro_task is coro_dynamic with the waiting moved into the fifo: its blocking put() and get() are
            sub-coroutines in dspsim and ordinary functions that call wait() in SystemC. coro_awaitable is
            the same call written as an awaitable object in dspsim, which needs no coroutine of its own.
        */
        constexpr int n = 16;
        constexpr int burst = 64;
        constexpr int depth = 16;
        const std::string group = "fifo/n=16,depth=16";
        const Params params{.iters = 3'000};
        add_pairs<FifoLink<ProducerMethod, ConsumerMethod, T, burst, depth>,
                  FifoLinkSC<ProducerMethodSC, ConsumerMethodSC, T, burst, depth>>(
            suite, group, "method", n, params);
        add_pairs<FifoLink<ProducerCoroDynamic, ConsumerCoroDynamic, T, burst, depth>,
                  FifoLinkSC<ProducerThreadDynamicSC, ConsumerThreadDynamicSC, T, burst, depth>>(
            suite, group, "coro_dynamic", n, params);
        add_pairs<FifoLink<ProducerCoroTask, ConsumerCoroTask, T, burst, depth>,
                  FifoLinkSC<ProducerThreadTaskSC, ConsumerThreadTaskSC, T, burst, depth>>(
            suite, group, "coro_task", n, params);
        add_pairs<FifoLink<ProducerCoroAwaitable, ConsumerCoroAwaitable, T, burst, depth>,
                  FifoLinkSC<ProducerThreadTaskSC, ConsumerThreadTaskSC, T, burst, depth>>(
            suite, group, "coro_awaitable", n, params);
    }
    return suite.main(argc, argv);
}
