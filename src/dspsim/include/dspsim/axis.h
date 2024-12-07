#pragma once
#include "dspsim/signal.h"
#include <deque>
#include <cmath>

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
        Signal<uint8_t> *m_axis_tid = nullptr;
        std::list<uint8_t> tid_pattern = {0};

    public:
        AxisTx(Signal<uint8_t> &clk,
               Signal<uint8_t> &rst,
               Signal<T> &m_axis_tdata,
               Signal<uint8_t> &m_axis_tvalid,
               Signal<uint8_t> &m_axis_tready,
               Signal<uint8_t> *m_axis_tid = nullptr,
               std::list<uint8_t> tid_pattern = std::list<uint8_t>{0});

        uint8_t _next_tid();

        void eval_step();

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

        void write_convert_scalar(double data, int q = 0)
        {
            int64_t fixed = data * std::pow(2, q);
            writei(fixed);
        }
        void write_convert_vector(std::vector<double> &data, int q = 0)
        {
            for (const auto &d : data)
            {
                write_convert_scalar(d, q);
            }
        }
        static std::shared_ptr<AxisTx> create(Signal<uint8_t> &clk,
                                              Signal<uint8_t> &rst,
                                              Signal<T> &m_axis_tdata,
                                              Signal<uint8_t> &m_axis_tvalid,
                                              Signal<uint8_t> &m_axis_tready,
                                              Signal<uint8_t> *m_axis_tid = nullptr,
                                              std::list<uint8_t> tid_pattern = {0})
        {
            auto axis_tx = std::make_shared<AxisTx>(clk, rst, m_axis_tdata, m_axis_tvalid, m_axis_tready, m_axis_tid, tid_pattern);
            axis_tx->context()->own_model(axis_tx);
            return axis_tx;
        }

    private:
        std::deque<T> buf;
        std::list<uint8_t>::iterator tid_it;
    };

    template <typename T>
    class AxisRx : public Model
    {
    protected:
        Signal<uint8_t> &clk;
        Signal<uint8_t> &rst;
        Signal<T> &s_axis_tdata;
        Signal<uint8_t> &s_axis_tvalid;
        Signal<uint8_t> &s_axis_tready;
        Signal<uint8_t> *s_axis_tid = nullptr;

    public:
        AxisRx(Signal<uint8_t> &clk,
               Signal<uint8_t> &rst,
               Signal<T> &s_axis_tdata,
               Signal<uint8_t> &s_axis_tvalid,
               Signal<uint8_t> &s_axis_tready,
               Signal<uint8_t> *s_axis_tid = nullptr);

        void set_tready(uint8_t value) { next_tready = value; }
        uint8_t get_tready() const { return s_axis_tready; }
        void eval_step();

        std::vector<T> read(bool clear = true);
        std::vector<uint8_t> read_tid(bool clear = true);

        static std::shared_ptr<AxisRx<T>> create(Signal<uint8_t> &clk,
                                                 Signal<uint8_t> &rst,
                                                 Signal<T> &s_axis_tdata,
                                                 Signal<uint8_t> &s_axis_tvalid,
                                                 Signal<uint8_t> &s_axis_tready,
                                                 Signal<uint8_t> *s_axis_tid = nullptr)
        {
            auto axis_rx = std::make_shared<AxisRx<T>>(clk, rst, s_axis_tdata, s_axis_tvalid, s_axis_tready, s_axis_tid);
            axis_rx->context()->own_model(axis_rx);
            return axis_rx;
        }

    protected:
        std::deque<T> rx_buf;
        std::deque<uint8_t> tid_buf;
        uint8_t next_tready = 0;
    };
} // namespace dspsim
