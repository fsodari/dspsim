#include <dspsim/axis.h>
#include <cmath>

namespace dspsim
{
    template <typename T>
    AxisTx<T>::AxisTx(Signal<uint8_t> &clk,
                      Signal<uint8_t> &rst,
                      Signal<T> &m_axis_tdata,
                      Signal<uint8_t> &m_axis_tvalid,
                      Signal<uint8_t> &m_axis_tready,
                      Signal<uint8_t> *m_axis_tid,
                      Signal<uint8_t> *m_axis_tlast,
                      std::list<uint8_t> tid_pattern)
        : clk(clk),
          rst(rst),
          m_axis_tdata(m_axis_tdata),
          m_axis_tvalid(m_axis_tvalid),
          m_axis_tready(m_axis_tready),
          m_axis_tid(m_axis_tid),
          m_axis_tlast(m_axis_tlast),
          tid_pattern(tid_pattern)
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
                if (m_axis_tlast)
                {
                    *m_axis_tlast = 0;
                }
            }

            if (rst)
            {
                m_axis_tvalid = 0;
            }
            else if (!buf.empty() && (!m_axis_tvalid || m_axis_tready))
            {
                m_axis_tdata = buf.front();
                auto _tid = _next_tid();
                if (m_axis_tid)
                {
                    *m_axis_tid = _tid;
                }

                if (m_axis_tlast && (this->tid_it == this->tid_pattern.begin()))
                {
                    *m_axis_tlast = 1;
                }
                m_axis_tvalid = 1;

                buf.pop_front();
            }
        }
    }

    template <typename T>
    void AxisTx<T>::write_command(T data)
    {
        buf.push_back(data);
    }

    template <typename T>
    void AxisTx<T>::write_command(std::vector<T> &data)
    {
        buf.insert(buf.end(), data.begin(), data.end());
    }

    template <typename T>
    void AxisTx<T>::writef_command(double data, int q)
    {
        T fixed = data * std::pow(2, q);
        write_command(fixed);
    }

    template <typename T>
    void AxisTx<T>::writef_command(std::vector<double> &data, int q)
    {
        for (const auto &d : data)
        {
            writef_command(d, q);
        }
    }

    template <typename T>
    int AxisTx<T>::block_wait(int timeout) const
    {
        for (int i = timeout; i != 0; --i)
        {
            context()->run(1);
            if (!busy())
            {
                return 1;
            }
        }

        return 0; // timed out.
    }

    template <typename T>
    void AxisTx<T>::write_block(T data, int timeout)
    {
        write_command(data);
        if (block_wait())
        {
            // Success
        }
        else
        {
            // timed out
        }
    }

    template <typename T>
    void AxisTx<T>::write_block(std::vector<T> &data, int timeout)
    {
        write_command(data);
        if (block_wait())
        {
            // Success
        }
        else
        {
            // timed out
        }
    }

    template <typename T>
    void AxisTx<T>::writef_block(double data, int q, int timeout)
    {
        writef_command(data, q);
        if (block_wait())
        {
            // Success
        }
        else
        {
            // timed out
        }
    }

    template <typename T>
    void AxisTx<T>::writef_block(std::vector<double> &data, int q, int timeout)
    {
        writef_command(data, q);
        if (block_wait())
        {
            // Success
        }
        else
        {
            // timed out
        }
    }

    template <typename T>
    std::shared_ptr<AxisTx<T>> AxisTx<T>::create(
        Signal<uint8_t> &clk,
        Signal<uint8_t> &rst,
        Signal<T> &m_axis_tdata,
        Signal<uint8_t> &m_axis_tvalid,
        Signal<uint8_t> &m_axis_tready,
        Signal<uint8_t> *m_axis_tid,
        Signal<uint8_t> *m_axis_tlast,
        std::list<uint8_t> tid_pattern)
    {
        // auto axis_tx = std::make_shared<AxisTx>(clk, rst, m_axis_tdata, m_axis_tvalid, m_axis_tready, m_axis_tid, m_axis_tlast, tid_pattern);
        // axis_tx->context()->own_model(axis_tx);
        // return axis_tx;
        // return Context::create_and_register<AxisTx<T>>(clk, rst, m_axis_tdata, m_axis_tvalid, m_axis_tready, m_axis_tid, m_axis_tlast, tid_pattern);
        return Model::create<AxisTx<T>>(clk, rst, m_axis_tdata, m_axis_tvalid, m_axis_tready, m_axis_tid, m_axis_tlast, tid_pattern);
    }

    template <typename T>
    AxisRx<T>::AxisRx(
        Signal<uint8_t> &clk,
        Signal<uint8_t> &rst,
        Signal<T> &s_axis_tdata,
        Signal<uint8_t> &s_axis_tvalid,
        Signal<uint8_t> &s_axis_tready,
        Signal<uint8_t> *s_axis_tid,
        Signal<uint8_t> *s_axis_tlast)
        : clk(clk),
          rst(rst),
          s_axis_tdata(s_axis_tdata),
          s_axis_tvalid(s_axis_tvalid),
          s_axis_tready(s_axis_tready),
          s_axis_tid(s_axis_tid),
          s_axis_tlast(s_axis_tlast)
    {
    }

    template <typename T>
    void AxisRx<T>::eval_step()
    {
        if (clk.posedge())
        {
            s_axis_tready = _next_tready;

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
    void AxisRx<T>::clear(int amount)
    {
        amount = (amount < 0 || amount > rx_buf.size()) ? rx_buf.size() : amount;
        rx_buf.erase(rx_buf.begin(), rx_buf.begin() + amount);
    }

    template <typename T>
    std::vector<T> AxisRx<T>::read_rx_buf(int amount)
    {
        amount = (amount < 0 || amount > rx_buf.size()) ? rx_buf.size() : amount;

        std::vector<T> result(rx_buf.begin(), rx_buf.begin() + amount);
        clear(amount);

        return result;
    }

    template <typename T>
    std::vector<double> AxisRx<T>::readf_rx_buf(int q, int amount)
    {
        amount = (amount < 0 || amount > rx_buf.size()) ? rx_buf.size() : amount;
        std::vector<double> result;
        result.reserve(amount);

        const double sf = std::pow(2, q);

        std::transform(rx_buf.begin(), rx_buf.end(), std::back_inserter(result), [&sf](const T &x)
                       { return static_cast<StdintSignedMap<T>::type>(x) / sf; });
        clear(amount);
        return result;
    }

    template <typename T>
    std::vector<uint8_t> AxisRx<T>::read_tid(int amount)
    {
        amount = amount < 0 ? tid_buf.size() : amount;

        std::vector<uint8_t> result(tid_buf.begin(), tid_buf.begin() + amount);
        tid_buf.erase(tid_buf.begin(), tid_buf.begin() + amount);
        return result;
    }

    template <typename T>
    int AxisRx<T>::block_wait(int n, int timeout)
    {
        for (int i = timeout; i != 0; --i)
        {
            context()->run(1);
            if (rx_buf.size() >= n)
            {
                return 1;
            }
        }

        return 0; // timed out
    }

    template <typename T>
    T AxisRx<T>::read_block(int timeout)
    {
        if (block_wait(1, timeout))
        {
            return read_rx_buf(1)[0];
        }
        else
        {
            // Exception?
            return 0;
        }
    }

    template <typename T>
    std::vector<T> AxisRx<T>::read_block(int n, int timeout)
    {
        if (block_wait(n, timeout))
        {
        }
        return read_rx_buf(n);
    }

    template <typename T>
    double AxisRx<T>::readf_block(int q, int timeout)
    {
        if (block_wait(1, timeout))
        {
            return readf_rx_buf(q, 1)[0];
        }
        return 0;
    }

    template <typename T>
    std::vector<double> AxisRx<T>::readf_block(int n, int q, int timeout)
    {
        if (block_wait(n, timeout))
        {
        }
        return readf_rx_buf(q, n);
    }

    template <typename T>
    std::shared_ptr<AxisRx<T>> AxisRx<T>::create(
        Signal<uint8_t> &clk,
        Signal<uint8_t> &rst,
        Signal<T> &s_axis_tdata,
        Signal<uint8_t> &s_axis_tvalid,
        Signal<uint8_t> &s_axis_tready,
        Signal<uint8_t> *s_axis_tid,
        Signal<uint8_t> *s_axis_tlast)
    {
        return Model::create<AxisRx<T>>(clk, rst, s_axis_tdata, s_axis_tvalid, s_axis_tready, s_axis_tid, s_axis_tlast);
    }

    // template class AxisTx<int8_t>;
    // template class AxisTx<int16_t>;
    // template class AxisTx<int32_t>;
    // template class AxisTx<int64_t>;
    template class AxisTx<uint8_t>;
    template class AxisTx<uint16_t>;
    template class AxisTx<uint32_t>;
    template class AxisTx<uint64_t>;
    // template class AxisRx<int8_t>;
    // template class AxisRx<int16_t>;
    // template class AxisRx<int32_t>;
    // template class AxisRx<int64_t>;
    template class AxisRx<uint8_t>;
    template class AxisRx<uint16_t>;
    template class AxisRx<uint32_t>;
    template class AxisRx<uint64_t>;

} // namespace dspsim
