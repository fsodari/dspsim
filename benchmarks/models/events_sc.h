/*
    SystemC counterparts of events.h. A dspsim event that a process notifies is an sc_event with a delta
    notification, notify(SC_ZERO_TIME): the processes waiting on it run in the next delta cycle.
*/
#pragma once
#include <systemc>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /// A one-place mailbox. See Mailbox in events.h.
    template <typename T>
    struct MailboxSC
    {
        void put(const T &item)
        {
            value = item;
            written.notify(sc_core::SC_ZERO_TIME);
        }

        sc_core::sc_event written;
        T value = 0;
    };

    /// SC_METHOD with static sensitivity.
    template <typename T>
    SC_MODULE(RingNodeMethodSC)
    {
        MailboxSC<T> inbox;
        MailboxSC<T> *next = nullptr;
        T sum = 0;

        SC_CTOR(RingNodeMethodSC)
        {
            SC_METHOD(eval);
            sensitive << inbox.written;
            dont_initialize();
        }

        void eval()
        {
            sum += inbox.value;
            next->put(sum);
        }
    };

    /// SC_METHOD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(RingNodeMethodDynamicSC)
    {
        MailboxSC<T> inbox;
        MailboxSC<T> *next = nullptr;
        T sum = 0;

        SC_CTOR(RingNodeMethodDynamicSC)
        {
            SC_METHOD(eval);
            sensitive << inbox.written;
            dont_initialize();
        }

        void eval()
        {
            sum += inbox.value;
            next->put(sum);
            next_trigger(inbox.written);
        }
    };

    /// SC_THREAD with static sensitivity.
    template <typename T>
    SC_MODULE(RingNodeThreadSC)
    {
        MailboxSC<T> inbox;
        MailboxSC<T> *next = nullptr;
        T sum = 0;

        SC_CTOR(RingNodeThreadSC)
        {
            SC_THREAD(run);
            sensitive << inbox.written;
        }

        void run()
        {
            while (true)
            {
                wait();
                sum += inbox.value;
                next->put(sum);
            }
        }
    };

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(RingNodeThreadDynamicSC)
    {
        MailboxSC<T> inbox;
        MailboxSC<T> *next = nullptr;
        T sum = 0;

        SC_CTOR(RingNodeThreadDynamicSC)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(inbox.written);
                sum += inbox.value;
                next->put(sum);
            }
        }
    };

    /// A ring of n nodes. See Ring in events.h.
    template <typename Node, typename T>
    SC_MODULE(RingSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        MailboxSC<T> arrived;
        std::vector<std::unique_ptr<Node>> nodes;

        RingSC(sc_core::sc_module_name, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                nodes.push_back(std::make_unique<Node>(("node" + std::to_string(i)).c_str()));
            }
            for (int i = 0; i < n; ++i)
            {
                nodes[i]->next = i + 1 < n ? &nodes[i + 1]->inbox : &arrived;
            }

            SC_METHOD(send);
            sensitive << clk.pos();
            dont_initialize();
            SC_METHOD(receive);
            sensitive << arrived.written;
            dont_initialize();
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

    /// What a broadcaster shares with its listeners. See Topic in events.h.
    template <typename T>
    struct TopicSC
    {
        sc_core::sc_event published;
        T message = 0;
        T tally = 0;
    };

    /// SC_METHOD with static sensitivity.
    template <typename T>
    SC_MODULE(ListenerMethodSC)
    {
        TopicSC<T> &topic;
        T weight;
        T sum = 0;

        ListenerMethodSC(sc_core::sc_module_name, TopicSC<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            SC_METHOD(eval);
            sensitive << topic.published;
            dont_initialize();
        }

        void eval()
        {
            sum += topic.message + weight;
            topic.tally += sum;
        }
    };

    /// SC_METHOD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(ListenerMethodDynamicSC)
    {
        TopicSC<T> &topic;
        T weight;
        T sum = 0;

        ListenerMethodDynamicSC(sc_core::sc_module_name, TopicSC<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            SC_METHOD(eval);
            sensitive << topic.published;
            dont_initialize();
        }

        void eval()
        {
            sum += topic.message + weight;
            topic.tally += sum;
            next_trigger(topic.published);
        }
    };

    /// SC_THREAD with static sensitivity.
    template <typename T>
    SC_MODULE(ListenerThreadSC)
    {
        TopicSC<T> &topic;
        T weight;
        T sum = 0;

        ListenerThreadSC(sc_core::sc_module_name, TopicSC<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            SC_THREAD(run);
            sensitive << topic.published;
        }

        void run()
        {
            while (true)
            {
                wait();
                sum += topic.message + weight;
                topic.tally += sum;
            }
        }
    };

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(ListenerThreadDynamicSC)
    {
        TopicSC<T> &topic;
        T weight;
        T sum = 0;

        ListenerThreadDynamicSC(sc_core::sc_module_name, TopicSC<T> &topic_, T weight_) : topic(topic_), weight(weight_)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(topic.published);
                sum += topic.message + weight;
                topic.tally += sum;
            }
        }
    };

    /// A broadcaster with n listeners. See Broadcast in events.h.
    template <typename Listener, typename T>
    SC_MODULE(BroadcastSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        TopicSC<T> topic;
        std::vector<std::unique_ptr<Listener>> listeners;

        BroadcastSC(sc_core::sc_module_name, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                listeners.push_back(
                    std::make_unique<Listener>(("listener" + std::to_string(i)).c_str(), topic, static_cast<T>(i)));
            }

            SC_METHOD(publish);
            sensitive << clk.pos();
            dont_initialize();
        }

        void publish()
        {
            out.write(topic.tally);
            topic.message = in.read();
            topic.published.notify(sc_core::SC_ZERO_TIME);
        }
    };

    /// The connection between an initiator and a target. See Transport in events.h.
    template <typename T>
    struct TransportSC
    {
        sc_core::sc_event request;
        sc_core::sc_event response;
        T data = 0;
    };

    /// SC_METHOD with static sensitivity.
    template <typename T>
    SC_MODULE(TargetMethodSC)
    {
        TransportSC<T> socket;
        T sum = 0;

        SC_CTOR(TargetMethodSC)
        {
            SC_METHOD(serve);
            sensitive << socket.request;
            dont_initialize();
        }

        void serve()
        {
            sum += socket.data;
            socket.data ^= sum;
            socket.response.notify(sc_core::SC_ZERO_TIME);
        }
    };

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(TargetThreadDynamicSC)
    {
        TransportSC<T> socket;
        T sum = 0;

        SC_CTOR(TargetThreadDynamicSC)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(socket.request);
                sum += socket.data;
                socket.data ^= sum;
                socket.response.notify(sc_core::SC_ZERO_TIME);
            }
        }
    };

    /// SC_METHODs with static sensitivity: one for the clock edge and one for the response.
    template <typename T>
    SC_MODULE(InitiatorMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        TransportSC<T> &socket;
        int trips;
        int remaining = 0;

        InitiatorMethodSC(sc_core::sc_module_name, TransportSC<T> &socket_, int trips_) : socket(socket_), trips(trips_)
        {
            SC_METHOD(start);
            sensitive << clk.pos();
            dont_initialize();
            SC_METHOD(advance);
            sensitive << socket.response;
            dont_initialize();
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
            socket.request.notify(sc_core::SC_ZERO_TIME);
        }
    };

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(InitiatorThreadDynamicSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        TransportSC<T> &socket;
        int trips;

        InitiatorThreadDynamicSC(sc_core::sc_module_name, TransportSC<T> &socket_, int trips_)
            : socket(socket_), trips(trips_)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(clk.posedge_event());
                T value = in.read();
                for (int i = 0; i < trips; ++i)
                {
                    socket.data = value;
                    socket.request.notify(sc_core::SC_ZERO_TIME);
                    wait(socket.response);
                    value = socket.data + 1;
                }
                out.write(value);
            }
        }
    };

    /// An initiator and its target as a stage for ChainSC. See Handshake in events.h.
    template <template <typename> class Initiator, template <typename> class Target, typename T, int TRIPS>
    SC_MODULE(HandshakeSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        Target<T> target{"target"};
        Initiator<T> initiator{"initiator", target.socket, TRIPS};

        SC_CTOR(HandshakeSC)
        {
            initiator.clk.bind(clk);
            initiator.in.bind(in);
            initiator.out.bind(out);
        }
    };

    /// A bounded queue with an event for each direction. See Fifo in events.h.
    template <typename T>
    class FifoSC
    {
    public:
        explicit FifoSC(size_t capacity) : capacity_(capacity) {}

        bool empty() const { return items_.empty(); }
        bool full() const { return items_.size() == capacity_; }

        void push(const T &item)
        {
            items_.push_back(item);
            written.notify(sc_core::SC_ZERO_TIME);
        }

        T pop()
        {
            const T item = items_.front();
            items_.pop_front();
            read.notify(sc_core::SC_ZERO_TIME);
            return item;
        }

        /// push() for an SC_THREAD: waits while the fifo is full.
        void put(T item)
        {
            while (full())
            {
                sc_core::wait(read);
            }
            push(item);
        }

        /// pop() for an SC_THREAD: waits while the fifo is empty.
        T get()
        {
            while (empty())
            {
                sc_core::wait(written);
            }
            return pop();
        }

        sc_core::sc_event written;
        sc_core::sc_event read;

    private:
        size_t capacity_;
        std::deque<T> items_;
    };

    /// SC_METHODs with static sensitivity: one for the clock edge, one for the room the reader makes.
    template <typename T>
    SC_MODULE(ProducerMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};

        FifoSC<T> &fifo;
        int burst;
        T item = 0;
        int remaining = 0;

        ProducerMethodSC(sc_core::sc_module_name, FifoSC<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            SC_METHOD(start);
            sensitive << clk.pos();
            dont_initialize();
            SC_METHOD(fill);
            sensitive << fifo.read;
            dont_initialize();
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

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(ProducerThreadDynamicSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};

        FifoSC<T> &fifo;
        int burst;

        ProducerThreadDynamicSC(sc_core::sc_module_name, FifoSC<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(clk.posedge_event());
                T item = in.read();
                int remaining = burst;
                while (remaining != 0)
                {
                    while (fifo.full())
                    {
                        wait(fifo.read);
                    }
                    fifo.push(item);
                    item = static_cast<T>(--remaining);
                }
            }
        }
    };

    /// SC_THREAD that calls the fifo's blocking put(), an ordinary function that waits.
    template <typename T>
    SC_MODULE(ProducerThreadTaskSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};

        FifoSC<T> &fifo;
        int burst;

        ProducerThreadTaskSC(sc_core::sc_module_name, FifoSC<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(clk.posedge_event());
                T item = in.read();
                int remaining = burst;
                while (remaining != 0)
                {
                    fifo.put(item);
                    item = static_cast<T>(--remaining);
                }
            }
        }
    };

    /// SC_METHOD with static sensitivity.
    template <typename T>
    SC_MODULE(ConsumerMethodSC)
    {
        sc_core::sc_out<T> out{"out"};

        FifoSC<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerMethodSC(sc_core::sc_module_name, FifoSC<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            SC_METHOD(drain);
            sensitive << fifo.written;
            dont_initialize();
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

    /// SC_THREAD with dynamic sensitivity.
    template <typename T>
    SC_MODULE(ConsumerThreadDynamicSC)
    {
        sc_core::sc_out<T> out{"out"};

        FifoSC<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerThreadDynamicSC(sc_core::sc_module_name, FifoSC<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                while (fifo.empty())
                {
                    wait(fifo.written);
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

    /// SC_THREAD that calls the fifo's blocking get(), an ordinary function that waits.
    template <typename T>
    SC_MODULE(ConsumerThreadTaskSC)
    {
        sc_core::sc_out<T> out{"out"};

        FifoSC<T> &fifo;
        int burst;
        T hash = 0;
        int count = 0;

        ConsumerThreadTaskSC(sc_core::sc_module_name, FifoSC<T> &fifo_, int burst_) : fifo(fifo_), burst(burst_)
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                const T item = fifo.get();
                hash = hash * 31 + item;
                if (++count == burst)
                {
                    count = 0;
                    out.write(hash);
                }
            }
        }
    };

    /// A producer and a consumer connected by a fifo, as a stage for ChainSC. See FifoLink in events.h.
    template <template <typename> class Producer, template <typename> class Consumer, typename T, int BURST, int DEPTH>
    SC_MODULE(FifoLinkSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        FifoSC<T> fifo{DEPTH};
        Producer<T> producer{"producer", fifo, BURST};
        Consumer<T> consumer{"consumer", fifo, BURST};

        SC_CTOR(FifoLinkSC)
        {
            producer.clk.bind(clk);
            producer.in.bind(in);
            consumer.out.bind(out);
        }
    };
}
