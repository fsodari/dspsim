#pragma once
#include <dspsim/module.h>
#include <ranges>
#include <concepts>
#include <algorithm>
#include <queue>
#include <spdlog/spdlog.h>

namespace dspsim
{
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
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
        }

        void eval()
        {
            if (clk.posedge())
            {
                // A valid transaction has occurred, pop the front of the FIFO.
                if (m_axis_tvalid.read() && m_axis_tready.read())
                {
                    // Clear the valid signal as the transaction has been accepted.
                    m_axis_tvalid.write(1);
                    fifo.pop_front();
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
    };
}
