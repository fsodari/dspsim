#pragma once
#include <dspsim/module.h>
#include <ranges>
#include <concepts>
#include <algorithm>
// #include <deque>
#include <queue>
#include <spdlog/spdlog.h>

namespace dspsim
{
    template <typename T>
    class AxisTx : public Module
    {
        using QueueType = std::queue<T>;

    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        Output<T> m_axis_tdata{"m_axis_tdata"};
        Output<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
        Input<uint8_t> m_axis_tready{"m_axis_tready"};

        QueueType fifo;

        AxisTx(ModuleName name) : Module(name)
        {
            always << clk.pos();
        }

        void eval() override
        {
            if (clk.posedge())
            {
                this->context()->logger->debug("AxisTx eval at posedge clk");
                // A valid transaction has occurred, pop the front of the FIFO.
                if (m_axis_tvalid.read() && m_axis_tready.read())
                {
                    // Clear the valid signal as the transaction has been accepted.
                    m_axis_tvalid.write(1);
                    // fifo.pop_front();
                    fifo.pop();
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
                    // Can we get here?
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

        // template <std::ranges::input_range R>
        //     requires std::convertible_to<std::ranges::range_reference_t<R>, T>
        // void append_range(R &&rg)
        // {
        //     // fifo.append_range(std::forward<R>(rg));
        //     fifo.insert(fifo.end(), std::ranges::begin(rg), std::ranges::end(rg));
        // }
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, T>
        void push_range(R &&rg)
        {
            // fifo.push_range(std::forward<R>(rg));
            // fifo.insert(fifo.end(), std::ranges::begin(rg), std::ranges::end(rg));
            for (const auto &a : rg)
            {
                fifo.push(a);
            }
        }

        // Don't allow removing data once queued?
        // void pop_front()
        // {
        //     fifo.pop_front();
        // }
        void pop()
        {
            fifo.pop();
        }
        void clear()
        {
            fifo.clear();
        }
    };
}
