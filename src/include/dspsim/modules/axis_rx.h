#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/coro.h>
#include <dspsim/event.h>
#include <algorithm>
#include <deque>
#include <vector>

namespace dspsim
{
    /*
        AXI-Stream sink. Accepts a beat on every clock edge where tvalid && tready, and queues the data in `fifo`.
        tready follows ready(): the sink has no throughput limit, so back-pressure is only what the user requests.

        Asynchronous use: `co_await axis_rx.receive(n, timeout)` resumes with the next n received samples
        (or fewer if the timeout elapsed), from any coroutine or via Context::run_until().
    */
    template <typename T>
    DSPSIM_MODULE(AxisRx)
    {
        using QueueType = std::deque<T>;

    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        Input<T> s_axis_tdata{"s_axis_tdata"};
        Input<uint8_t> s_axis_tvalid{"s_axis_tvalid"};
        Output<uint8_t> s_axis_tready{"s_axis_tready"};

        QueueType fifo;
        uint8_t _ready = 0;

        DSPSIM_CTOR(AxisRx)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());

                if (s_axis_tvalid.read() && s_axis_tready.read())
                {
                    fifo.push_back(s_axis_tdata.read());
                    received_.notify();
                }

                // No limit to buffer size or throughput, so set ready when user requests.
                s_axis_tready.write(_ready);
            }
        }

        // Triggers after every accepted beat, once the data is in the fifo.
        SensitivityEvent &received() { return received_; }

        /*
            Wait until n samples are available, then remove and return them. With a timeout (> 0), returns the
            samples available at the deadline, so fewer than n means the receive timed out.
            Does not drive tready: set ready(1) to accept data.
        */
        Task<std::vector<T>> receive(size_t n, uint64_t timeout = 0)
        {
            const uint64_t deadline = context()->time() + timeout;
            while (fifo.size() < n)
            {
                if (timeout == 0)
                {
                    co_await wait(received_);
                }
                else
                {
                    const uint64_t now = context()->time();
                    if (now >= deadline)
                    {
                        break;
                    }
                    if ((co_await wait(received_, deadline - now)) == WaitResult::Timeout)
                    {
                        break;
                    }
                }
            }
            co_return take(n);
        }

        // Remove and return up to n samples from the front of the fifo.
        std::vector<T> take(size_t n)
        {
            n = std::min(n, fifo.size());
            std::vector<T> data(fifo.begin(), fifo.begin() + n);
            fifo.erase(fifo.begin(), fifo.begin() + n);
            return data;
        }

        void ready(uint8_t r)
        {
            _ready = r;
        }
        uint8_t ready() const
        {
            return _ready;
        }
        size_t size() const
        {
            return fifo.size();
        }
        bool empty() const
        {
            return fifo.empty();
        }
        void clear()
        {
            fifo.clear();
        }

    private:
        SensitivityEvent received_{context()};
    };
}
