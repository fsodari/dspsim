#pragma once
#include "dspsim/signal.h"
#include <deque>

namespace dspsim
{
    template <typename T>
    struct Axis
    {
        SignalPtr<T> tdata;
        SignalPtr<uint8_t> tvalid;
        SignalPtr<uint8_t> tready;

        Axis() : tdata(create<Signal<T>>()), tvalid(create<Signal<uint8_t>>()), tready(create<Signal<uint8_t>>())
        {
        }
    };

    template <typename T>
    class AxisTx : public Model
    {
    protected:
        Signal<uint8_t> &clk;
        Signal<uint8_t> &rst;
        Signal<T> &m_axis_tdata;
        Signal<uint8_t> &m_axis_tvalid;
        Signal<uint8_t> &m_axis_tready;

    public:
        AxisTx(Signal<uint8_t> &clk, Signal<uint8_t> &rst, Signal<T> &m_axis_tdata, Signal<uint8_t> &m_axis_tvalid, Signal<uint8_t> &m_axis_tready)
            : clk(clk), rst(rst), m_axis_tdata(m_axis_tdata), m_axis_tvalid(m_axis_tvalid), m_axis_tready(m_axis_tready)
        {
        }

        void eval_step()
        {
            if (clk.posedge())
            {
                if (m_axis_tvalid && m_axis_tready)
                {
                    m_axis_tvalid = 0;
                }

                if (!buf.empty() && (!m_axis_tvalid || m_axis_tready))
                {
                    m_axis_tdata = buf.front();
                    m_axis_tvalid = 1;

                    buf.pop_front();
                }
                if (rst)
                {
                    m_axis_tvalid = 0;
                }
            }
        }

        void write(T data)
        {
            buf.push_back(data);
        }
        template <typename Iter>
        void insert(Iter begin, Iter end)
        {
            buf.insert(buf.end(), begin, end);
        }
        void write(std::vector<T> &data)
        {
            insert(data.begin(), data.end());
        }
        // Python helpers.
        void writei(T data) { write(data); }
        void writev(std::vector<T> &data) { write(data); }

    private:
        std::deque<T> buf;
    };
} // namespace dspsim
