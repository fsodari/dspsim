#pragma once
#include <dspsim/module.h>
#include <ranges>
#include <spdlog/spdlog.h>

namespace dspsim
{
    template <typename T>
    class AxisTx : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        Output<T> m_axis_tdata{"m_axis_tdata"};
        Output<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
        Input<uint8_t> m_axis_tready{"m_axis_tready"};

        std::deque<T> fifo;

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
                    // Can we get here?
                    m_axis_tvalid.write(0);
                }

                if (rst.read() == 1)
                {
                    m_axis_tvalid.write(0);
                }
            }
        }

        void send(T data)
        {
            std::ranges::copy(std::views::single(data), std::back_inserter(fifo));
        }

        void send(const std::ranges::range auto &data)
        {
            std::ranges::copy(data, std::back_inserter(fifo));
        }

        void send(const std::initializer_list<T> &data)
        {
            std::ranges::copy(data, std::back_inserter(fifo));
        }
    };
}
