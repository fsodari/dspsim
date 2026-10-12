/*
    dspsim models for the events benchmarks: designs whose processes talk to each other through events
    that the model notifies itself (SensitivityEvent::notify()), where the other suites only use the
    events a signal raises when it changes. events_sc.h has the SystemC counterparts.

    Each design is a small channel (a mailbox, a topic, a transport, a fifo: plain data plus the events
    that announce a change of it) and modules that call it. A notification wakes the subscribers in the
    next delta cycle, so data handed over with it is read one delta cycle after it was stored, by a
    process that was already waiting.

    In every design here, a process starts to wait on an event in an earlier delta cycle than the one in
    which the event is notified, and the processes of one delta cycle share no data, or only add to a
    common sum. The results therefore depend neither on the order in which an engine evaluates the
    processes of a delta cycle, nor on what it does with a notification when the waiting process only
    subscribes later in the same evaluation phase.
*/
#pragma once
#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /*
        Token ring.

        Stands for a chain of transaction-level models that hand a transaction on, each one waking the
        next. Stresses the path from notify() to the evaluation of a single subscriber: every notification
        wakes one process, in a delta cycle of its own.
    */
    /// A one-place mailbox: put() stores a value and notifies the reader.
    template <typename T>
    struct Mailbox
    {
        explicit Mailbox(dspsim::Context *context) : written(context) {}

        void put(const T &item)
        {
            value = item;
            written.notify();
        }

        dspsim::SensitivityEvent written;
        T value = 0;
    };

    /*
        A node of the ring, once per process kind: it waits for the token in its mailbox, adds it to a
        running sum and passes the sum on. The four produce the same output.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(RingNodeMethod)
    {
    public:
        Mailbox<T> inbox{context()};
        /// The mailbox of the next node, set by the ring.
        Mailbox<T> *next = nullptr;
        T sum = 0;

        DSPSIM_CTOR(RingNodeMethod)
        {
            // A node has nothing to do until the token arrives, so skip the initial evaluation.
            DSPSIM_METHOD(eval)
                ->always(inbox.written)
                ->initialize(false);
        }

        void eval()
        {
            sum += inbox.value;
            next->put(sum);
        }
    };

    /// Method process with dynamic sensitivity: next_trigger() on every evaluation.
    template <typename T>
    DSPSIM_MODULE(RingNodeMethodDynamic)
    {
    public:
        Mailbox<T> inbox{context()};
        Mailbox<T> *next = nullptr;
        T sum = 0;

        DSPSIM_CTOR(RingNodeMethodDynamic)
        {
            // Static sensitivity only triggers the first evaluation. After that, next_trigger() overrides it.
            DSPSIM_METHOD(eval)
                ->always(inbox.written)
                ->initialize(false);
        }

        void eval()
        {
            sum += inbox.value;
            next->put(sum);
            next_trigger(inbox.written);
        }
    };

    /// Coroutine process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(RingNodeCoro)
    {
    public:
        Mailbox<T> inbox{context()};
        Mailbox<T> *next = nullptr;
        T sum = 0;

        DSPSIM_CTOR(RingNodeCoro)
        {
            DSPSIM_CORO(run)
                ->always(inbox.written);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait();
                sum += inbox.value;
                next->put(sum);
            }
        }
    };

    /// Coroutine process with dynamic sensitivity: it subscribes to the event again on every wait.
    template <typename T>
    DSPSIM_MODULE(RingNodeCoroDynamic)
    {
    public:
        Mailbox<T> inbox{context()};
        Mailbox<T> *next = nullptr;
        T sum = 0;

        DSPSIM_CTOR(RingNodeCoroDynamic)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(inbox.written);
                sum += inbox.value;
                next->put(sum);
            }
        }
    };

    /*
        A ring of n nodes. On every clock edge the token starts a revolution with the input value, and
        what comes back after the last node is the output: n + 1 notifications per clock cycle, one per
        delta cycle.

        The two processes that send and receive the token are method processes in every kind.
    */
    template <typename Node, typename T>
    DSPSIM_MODULE(Ring)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        /// Where the token arrives after the last node.
        Mailbox<T> arrived{context()};
        std::vector<std::unique_ptr<Node>> nodes;

        Ring(::dspsim::ModuleName, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                nodes.push_back(std::make_unique<Node>("node" + std::to_string(i)));
            }
            for (int i = 0; i < n; ++i)
            {
                nodes[i]->next = i + 1 < n ? &nodes[i + 1]->inbox : &arrived;
            }

            DSPSIM_METHOD(send)
                ->always(clk.pos())
                ->initialize(false);
            DSPSIM_METHOD(receive)
                ->always(arrived.written)
                ->initialize(false);
        }

        void send()
        {
            nodes.front()->inbox.put(in.read());
        }

        void receive()
        {
            out.write(arrived.value);
        }
    };

    /*
        Broadcast.

        Stands for an event that many models react to: a frame sync, an interrupt, a monitor that hands
        every transaction to its subscribers. Stresses an event with many subscribers: one notification
        makes all of them runnable in the same delta cycle, and the dynamic kinds subscribe again each time.
    */
    /*
        What a broadcaster shares with its listeners: the event, the message, and a tally of what the
        listeners made of it. Adding to the tally is commutative, so the result does not depend on the
        order in which the listeners are evaluated.
    */
    template <typename T>
    struct Topic
    {
        explicit Topic(dspsim::Context *context) : published(context) {}

        dspsim::SensitivityEvent published;
        T message = 0;
        T tally = 0;
    };

    /*
        A listener, once per process kind: on every message it updates a sum of its own and adds it to the
        tally. The weight makes every listener contribute something different.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(ListenerMethod)
    {
    public:
        Topic<T> &topic;
        T weight;
        T sum = 0;

        ListenerMethod(::dspsim::ModuleName, Topic<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            DSPSIM_METHOD(eval)
                ->always(topic.published)
                ->initialize(false);
        }

        void eval()
        {
            sum += topic.message + weight;
            topic.tally += sum;
        }
    };

    /// Method process with dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(ListenerMethodDynamic)
    {
    public:
        Topic<T> &topic;
        T weight;
        T sum = 0;

        ListenerMethodDynamic(::dspsim::ModuleName, Topic<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            // Static sensitivity only triggers the first evaluation. After that, next_trigger() overrides it.
            DSPSIM_METHOD(eval)
                ->always(topic.published)
                ->initialize(false);
        }

        void eval()
        {
            sum += topic.message + weight;
            topic.tally += sum;
            next_trigger(topic.published);
        }
    };

    /// Coroutine process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(ListenerCoro)
    {
    public:
        Topic<T> &topic;
        T weight;
        T sum = 0;

        ListenerCoro(::dspsim::ModuleName, Topic<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            DSPSIM_CORO(run)
                ->always(topic.published);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait();
                sum += topic.message + weight;
                topic.tally += sum;
            }
        }
    };

    /// Coroutine process with dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(ListenerCoroDynamic)
    {
    public:
        Topic<T> &topic;
        T weight;
        T sum = 0;

        ListenerCoroDynamic(::dspsim::ModuleName, Topic<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(topic.published);
                sum += topic.message + weight;
                topic.tally += sum;
            }
        }
    };

    /*
        A broadcaster with n listeners. On every clock edge it publishes the input value, and the output
        is the tally of the messages before it: one notification and n evaluations per clock cycle.

        The publishing process is a method process in every kind.
    */
    template <typename Listener, typename T>
    DSPSIM_MODULE(Broadcast)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        Topic<T> topic{context()};
        std::vector<std::unique_ptr<Listener>> listeners;

        Broadcast(::dspsim::ModuleName, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                listeners.push_back(
                    std::make_unique<Listener>("listener" + std::to_string(i), topic, static_cast<T>(i)));
            }

            DSPSIM_METHOD(publish)
                ->always(clk.pos())
                ->initialize(false);
        }

        void publish()
        {
            // The listeners ran in the delta cycle after the previous message, so its tally is complete.
            out.write(topic.tally);
            topic.message = in.read();
            topic.published.notify();
        }
    };

    /*
        Handshake.

        Stands for a transaction-level bus: an initiator sends a request to a target and waits for the
        response, several times per clock cycle. Stresses a pair of processes that wake each other in
        turn, each waiting on one event while it notifies the other.
    */
    /// The connection between an initiator and a target: an event per direction and the transaction.
    template <typename T>
    struct Transport
    {
        explicit Transport(dspsim::Context *context) : request(context), response(context) {}

        dspsim::SensitivityEvent request;
        dspsim::SensitivityEvent response;
        /// The initiator stores the request here and the target replaces it with the response.
        T data = 0;
    };

    /*
        A target, once per process kind: it keeps the sum of the requests and answers each one with the
        request xored with that sum.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(TargetMethod)
    {
    public:
        Transport<T> socket{context()};
        T sum = 0;

        DSPSIM_CTOR(TargetMethod)
        {
            DSPSIM_METHOD(serve)
                ->always(socket.request)
                ->initialize(false);
        }

        void serve()
        {
            sum += socket.data;
            socket.data ^= sum;
            socket.response.notify();
        }
    };

    /// Coroutine process with dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(TargetCoroDynamic)
    {
    public:
        Transport<T> socket{context()};
        T sum = 0;

        DSPSIM_CTOR(TargetCoroDynamic)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(socket.request);
                sum += socket.data;
                socket.data ^= sum;
                socket.response.notify();
            }
        }
    };

    /*
        An initiator, once per process kind. A clock edge starts a burst of `trips` round trips: the first
        request is the input value, each later one is the response before it plus one, and the last
        response plus one is the output.
    */
    /// Method processes with static sensitivity: one for the clock edge and one for the response.
    template <typename T>
    DSPSIM_MODULE(InitiatorMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        Transport<T> &socket;
        int trips;
        /// Responses the burst is still waiting for.
        int remaining = 0;

        InitiatorMethod(::dspsim::ModuleName, Transport<T> &socket_, int trips_) : socket(socket_), trips(trips_)
        {
            DSPSIM_METHOD(start)
                ->always(clk.pos())
                ->initialize(false);
            DSPSIM_METHOD(advance)
                ->always(socket.response)
                ->initialize(false);
        }

        void start()
        {
            remaining = trips;
            send(in.read());
        }

        void advance()
        {
            const T value = socket.data + 1;
            if (--remaining != 0)
            {
                send(value);
            }
            else
            {
                out.write(value);
            }
        }

        void send(T value)
        {
            socket.data = value;
            socket.request.notify();
        }
    };

    /// Coroutine process with dynamic sensitivity: the burst is a loop.
    template <typename T>
    DSPSIM_MODULE(InitiatorCoroDynamic)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        Transport<T> &socket;
        int trips;

        InitiatorCoroDynamic(::dspsim::ModuleName, Transport<T> &socket_, int trips_) : socket(socket_), trips(trips_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(clk.pos());
                T value = in.read();
                for (int i = 0; i < trips; ++i)
                {
                    socket.data = value;
                    socket.request.notify();
                    co_await wait(socket.response);
                    value = socket.data + 1;
                }
                out.write(value);
            }
        }
    };

    /*
        An initiator and its target as a stage with the ports clk, in and out, which Chain (chain.h) puts in
        series: every stage runs its burst after the same clock edge, so each delta cycle evaluates one
        process per stage, each woken by an event of its own. A round trip takes two delta cycles.
    */
    template <template <typename> class Initiator, template <typename> class Target, typename T, int TRIPS>
    DSPSIM_MODULE(Handshake)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        Target<T> target{"target"};
        Initiator<T> initiator{"initiator", target.socket, TRIPS};

        DSPSIM_CTOR(Handshake)
        {
            initiator.clk.bind(clk);
            initiator.in.bind(in);
            initiator.out.bind(out);
        }
    };

    /*
        Fifo.

        Stands for two models connected by a queue: the pattern of AxisRx::received() and of sc_fifo. The
        writer blocks while the fifo is full and the reader while it is empty, and each is woken by the
        other's event. Stresses events that are notified more often than they wake anybody: every push and
        every pop notifies, several times per evaluation.
    */
    /// A bounded queue with an event for each direction.
    template <typename T>
    class Fifo
    {
    public:
        Fifo(dspsim::Context *context, size_t capacity)
            : written(context), read(context), context_(context), capacity_(capacity) {}

        dspsim::Context *context() const { return context_; }
        bool empty() const { return items_.empty(); }
        bool full() const { return items_.size() == capacity_; }

        /// Append an item. The fifo must not be full.
        void push(const T &item)
        {
            items_.push_back(item);
            written.notify();
        }

        /// Remove and return the oldest item. The fifo must not be empty.
        T pop()
        {
            const T item = items_.front();
            items_.pop_front();
            read.notify();
            return item;
        }

        /// push() for a coroutine process: waits while the fifo is full.
        dspsim::Task<> put(T item)
        {
            while (full())
            {
                co_await context_->wait(read);
            }
            push(item);
        }

        /// pop() for a coroutine process: waits while the fifo is empty.
        dspsim::Task<T> get()
        {
            while (empty())
            {
                co_await context_->wait(written);
            }
            co_return pop();
        }

        /// Notified by every push().
        dspsim::SensitivityEvent written;
        /// Notified by every pop().
        dspsim::SensitivityEvent read;

    private:
        dspsim::Context *context_;
        size_t capacity_;
        std::deque<T> items_;
    };

    /*
        put() and get() once more, as awaitables instead of coroutines: `co_await FifoPut{fifo, item}`.

        A Task is a coroutine of its own, with a frame that is created and destroyed around every call. An
        awaitable is an ordinary object: when the fifo has room, await_ready() says so and the calling
        coroutine goes on without suspending, so the call is a few inlined instructions. When it has none,
        await_suspend() waits for the reader's event with the engine's own wait, which suspends the calling
        coroutine itself.

        An awaitable is resumed once, so it cannot wait in a loop the way put() does. One wake-up is enough
        here because the fifo has one writer and one reader: nobody else can take the room that a pop() made.
    */
    template <typename T>
    class FifoPut
    {
    public:
        FifoPut(Fifo<T> &fifo, const T &item) : fifo_(fifo), item_(item) {}

        bool await_ready() const { return !fifo_.full(); }
        void await_suspend(std::coroutine_handle<> handle)
        {
            // The wait subscribes the calling process to the event, and records where to resume it.
            fifo_.context()->wait(fifo_.read).await_suspend(handle);
        }
        void await_resume() { fifo_.push(item_); }

    private:
        Fifo<T> &fifo_;
        T item_;
    };

    template <typename T>
    class FifoGet
    {
    public:
        explicit FifoGet(Fifo<T> &fifo) : fifo_(fifo) {}

        bool await_ready() const { return !fifo_.empty(); }
        void await_suspend(std::coroutine_handle<> handle)
        {
            fifo_.context()->wait(fifo_.written).await_suspend(handle);
        }
        T await_resume() { return fifo_.pop(); }

    private:
        Fifo<T> &fifo_;
    };

    /*
        A producer, once per process kind. A clock edge starts a burst of `burst` items, which it pushes
        as fast as the fifo takes them: the input value, then a count-down. Only the first item depends on
        the input, because an even number of items that all did would cancel in the low bits of the
        consumer's hash, and a chain of stages would lose the input after a few of them.
    */
    /// Method processes with static sensitivity: one for the clock edge, one for the room the reader makes.
    template <typename T>
    DSPSIM_MODULE(ProducerMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};

        Fifo<T> &fifo;
        int burst;
        /// The next item, and the number of items of the burst that are not in the fifo yet.
        T item = 0;
        int remaining = 0;

        ProducerMethod(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_METHOD(start)
                ->always(clk.pos())
                ->initialize(false);
            DSPSIM_METHOD(fill)
                ->always(fifo.read)
                ->initialize(false);
        }

        void start()
        {
            item = in.read();
            remaining = burst;
            fill();
        }

        void fill()
        {
            while (remaining != 0 && !fifo.full())
            {
                fifo.push(item);
                item = static_cast<T>(--remaining);
            }
        }
    };

    /// Coroutine process with dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(ProducerCoroDynamic)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};

        Fifo<T> &fifo;
        int burst;

        ProducerCoroDynamic(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(clk.pos());
                T item = in.read();
                int remaining = burst;
                while (remaining != 0)
                {
                    while (fifo.full())
                    {
                        co_await wait(fifo.read);
                    }
                    fifo.push(item);
                    item = static_cast<T>(--remaining);
                }
            }
        }
    };

    /// Coroutine process that calls the fifo's blocking put(), a sub-coroutine.
    template <typename T>
    DSPSIM_MODULE(ProducerCoroTask)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};

        Fifo<T> &fifo;
        int burst;

        ProducerCoroTask(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(clk.pos());
                T item = in.read();
                int remaining = burst;
                while (remaining != 0)
                {
                    co_await fifo.put(item);
                    item = static_cast<T>(--remaining);
                }
            }
        }
    };

    /// Coroutine process that awaits the fifo's awaitable, FifoPut.
    template <typename T>
    DSPSIM_MODULE(ProducerCoroAwaitable)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};

        Fifo<T> &fifo;
        int burst;

        ProducerCoroAwaitable(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(clk.pos());
                T item = in.read();
                int remaining = burst;
                while (remaining != 0)
                {
                    co_await FifoPut<T>{fifo, item};
                    item = static_cast<T>(--remaining);
                }
            }
        }
    };

    /*
        A consumer, once per process kind. It folds the items into a hash that depends on their order, and
        outputs the hash after every `burst` items.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(ConsumerMethod)
    {
    public:
        dspsim::Output<T> out{"out"};

        Fifo<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerMethod(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_METHOD(drain)
                ->always(fifo.written)
                ->initialize(false);
        }

        void drain()
        {
            while (!fifo.empty())
            {
                hash = hash * 31 + fifo.pop();
                if (++count == burst)
                {
                    count = 0;
                    out.write(hash);
                }
            }
        }
    };

    /// Coroutine process with dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(ConsumerCoroDynamic)
    {
    public:
        dspsim::Output<T> out{"out"};

        Fifo<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerCoroDynamic(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                while (fifo.empty())
                {
                    co_await wait(fifo.written);
                }
                hash = hash * 31 + fifo.pop();
                if (++count == burst)
                {
                    count = 0;
                    out.write(hash);
                }
            }
        }
    };

    /// Coroutine process that calls the fifo's blocking get(), a sub-coroutine.
    template <typename T>
    DSPSIM_MODULE(ConsumerCoroTask)
    {
    public:
        dspsim::Output<T> out{"out"};

        Fifo<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerCoroTask(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                const T item = co_await fifo.get();
                hash = hash * 31 + item;
                if (++count == burst)
                {
                    count = 0;
                    out.write(hash);
                }
            }
        }
    };

    /// Coroutine process that awaits the fifo's awaitable, FifoGet.
    template <typename T>
    DSPSIM_MODULE(ConsumerCoroAwaitable)
    {
    public:
        dspsim::Output<T> out{"out"};

        Fifo<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerCoroAwaitable(::dspsim::ModuleName, Fifo<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                const T item = co_await FifoGet<T>{fifo};
                hash = hash * 31 + item;
                if (++count == burst)
                {
                    count = 0;
                    out.write(hash);
                }
            }
        }
    };

    /*
        A producer and a consumer connected by a fifo, as a stage with the ports clk, in and out for Chain
        (chain.h). The fifo holds DEPTH items and a burst is longer, so the two take turns: the producer
        fills the fifo and waits for room, the consumer empties it and waits for data. They are never
        evaluated in the same delta cycle, which is what makes it safe for them to share the queue.
    */
    template <template <typename> class Producer, template <typename> class Consumer, typename T, int BURST, int DEPTH>
    DSPSIM_MODULE(FifoLink)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        Fifo<T> fifo{context(), DEPTH};
        Producer<T> producer{"producer", fifo, BURST};
        Consumer<T> consumer{"consumer", fifo, BURST};

        DSPSIM_CTOR(FifoLink)
        {
            producer.clk.bind(clk);
            producer.in.bind(in);
            consumer.out.bind(out);
        }
    };
}
