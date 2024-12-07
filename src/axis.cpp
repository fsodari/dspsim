#include <dspsim/axis.h>

namespace dspsim
{
    template <typename T>
    AxisTx<T>::AxisTx(Signal<uint8_t> &clk,
                      Signal<uint8_t> &rst,
                      Signal<T> &m_axis_tdata,
                      Signal<uint8_t> &m_axis_tvalid,
                      Signal<uint8_t> &m_axis_tready,
                      Signal<uint8_t> *m_axis_tid,
                      std::list<uint8_t> tid_pattern)
        : clk(clk),
          rst(rst), m_axis_tdata(m_axis_tdata), m_axis_tvalid(m_axis_tvalid), m_axis_tready(m_axis_tready), m_axis_tid(m_axis_tid), tid_pattern(tid_pattern)
    {
        this->tid_it = this->tid_pattern.begin();
    }

    template <typename T>
    uint8_t AxisTx<T>::_next_tid()
    {
        auto id = *tid_it;
        tid_it++;
        if (tid_it == tid_pattern.end())
        {
            tid_it = tid_pattern.begin();
        }
        return id;
    }

    template <typename T>
    void AxisTx<T>::eval_step()
    {
        if (clk.posedge())
        {
            if (m_axis_tvalid && m_axis_tready)
            {
                m_axis_tvalid = 0;
            }

            if (rst)
            {
                m_axis_tvalid = 0;
            }
            else if (!buf.empty() && (!m_axis_tvalid || m_axis_tready))
            {
                m_axis_tdata = buf.front();
                if (m_axis_tid)
                {
                    *m_axis_tid = _next_tid();
                }
                m_axis_tvalid = 1;

                buf.pop_front();
            }
        }
    }

    template <typename T>
    AxisRx<T>::AxisRx(Signal<uint8_t> &clk,
                      Signal<uint8_t> &rst,
                      Signal<T> &s_axis_tdata,
                      Signal<uint8_t> &s_axis_tvalid,
                      Signal<uint8_t> &s_axis_tready,
                      Signal<uint8_t> *s_axis_tid)
        : clk(clk),
          rst(rst),
          s_axis_tdata(s_axis_tdata),
          s_axis_tvalid(s_axis_tvalid),
          s_axis_tready(s_axis_tready),
          s_axis_tid(s_axis_tid)
    {
    }

    template <typename T>
    void AxisRx<T>::eval_step()
    {
        if (clk.posedge())
        {
            s_axis_tready = next_tready;

            if (rst)
            {
                s_axis_tready = 0;
            }
            else if (s_axis_tvalid && s_axis_tready)
            {
                rx_buf.push_back(s_axis_tdata);
                if (s_axis_tid)
                {
                    tid_buf.push_back(*s_axis_tid);
                }
            }
        }
    }

    template <typename T>
    std::vector<T> AxisRx<T>::read(bool clear)
    {
        std::vector<T> result(rx_buf.begin(), rx_buf.end());
        if (clear)
        {
            rx_buf.clear();
        }
        return result;
    }

    template <typename T>
    std::vector<uint8_t> AxisRx<T>::read_tid(bool clear)
    {
        std::vector<uint8_t> result(tid_buf.begin(), tid_buf.end());
        if (clear)
        {
            tid_buf.clear();
        }
        return result;
    }

    template class AxisTx<uint8_t>;
    template class AxisTx<uint16_t>;
    template class AxisTx<uint32_t>;
    template class AxisTx<uint64_t>;
    template class AxisRx<uint8_t>;
    template class AxisRx<uint16_t>;
    template class AxisRx<uint32_t>;
    template class AxisRx<uint64_t>;

} // namespace dspsim
