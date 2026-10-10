#pragma once
#include <dspsim/module.h>
#include <dspsim/coro.h>
#include <dspsim/event.h>
#include <ranges>
#include <concepts>
#include <algorithm>
#include <queue>
#include <vector>

namespace dspsim
{
    /*
        AXI-Stream source. Presents the front of `fifo` on the bus and pops it when the sink accepts it.

        Asynchronous use: `co_await axis_tx.send(data, timeout)` queues the data and resumes once the sink has
        accepted all of it (true), or when the timeout elapses first (false).
    */
    template <typename T>
    DSPSIM_MODULE(AxisTx)
    {
        using QueueType = std::deque<T>;

    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        Output<T> m_axis_tdata{"m_axis_tdata"};
        Output<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
        Input<uint8_t> m_axis_tready{"m_axis_tready"};

        QueueType fifo;

        DSPSIM_CTOR(AxisTx)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());

                // A valid transaction has occurred, pop the front of the FIFO.
                if (m_axis_tvalid.read() && m_axis_tready.read())
                {
                    // Clear the valid signal as the transaction has been accepted.
                    m_axis_tvalid.write(1);
                    fifo.pop_front();
                    sent_.notify();
                }

                // Bus is waiting for downstream to be ready.
                if (!m_axis_tready.read() && m_axis_tvalid.read())
                {
                }
                // We have data to send.
                else if (!fifo.empty())
                {
                    m_axis_tvalid.write(1);
                    m_axis_tdata.write(fifo.front());
                }
                else
                {
                    // No data to send, set valid low.
                    m_axis_tvalid.write(0);
                }

                if (rst.read() == 1)
                {
                    m_axis_tvalid.write(0);
                }
            }
        }

        // Triggers after every beat accepted by the sink.
        SensitivityEvent &sent() { return sent_; }

        /*
            Queue the data and wait until the sink has accepted everything queued. Returns true when the fifo
            drained, false if the timeout (> 0) elapsed first, leaving the rest queued.
        */
        Task<bool> send(std::vector<T> data, uint64_t timeout = 0)
        {
            push_range(data);
            co_return co_await drain(timeout);
        }

        // Wait until the fifo is empty. Returns false if the timeout (> 0) elapsed first.
        Task<bool> drain(uint64_t timeout = 0)
        {
            const uint64_t deadline = context()->time() + timeout;
            while (!fifo.empty())
            {
                if (timeout == 0)
                {
                    co_await wait(sent_);
                }
                else
                {
                    const uint64_t now = context()->time();
                    if (now >= deadline)
                    {
                        break;
                    }
                    if ((co_await wait(sent_, deadline - now)) == WaitResult::Timeout)
                    {
                        break;
                    }
                }
            }
            co_return fifo.empty();
        }

        // Supports queue-like operations.
        const T &front() const
        {
            return fifo.front();
        }
        const T &back() const
        {
            return fifo.back();
        }
        bool empty() const
        {
            return fifo.empty();
        }
        size_t size() const
        {
            return fifo.size();
        }
        void push_back(const T &element)
        {
            fifo.push_back(element);
        }

        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, T>
        void push_range(R && rg)
        {
            for (const auto &a : rg)
            {
                fifo.push_back(a);
            }
        }

        void pop()
        {
            fifo.pop_front();
        }
        void clear()
        {
            fifo.clear();
        }

    private:
        SensitivityEvent sent_{context()};
    };
}
