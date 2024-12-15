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

        void eval_step();

        void write(T data);
        void write(std::vector<T> &data);
        void writef(double data, int q = 0);
        void writef(std::vector<double> &data, int q = 0);

        // Create as shared_ptr.
        static std::shared_ptr<AxisTx> create(
            Signal<uint8_t> &clk,
            Signal<uint8_t> &rst,
            Signal<T> &m_axis_tdata,
            Signal<uint8_t> &m_axis_tvalid,
            Signal<uint8_t> &m_axis_tready,
            Signal<uint8_t> *m_axis_tid = nullptr,
            std::list<uint8_t> tid_pattern = {0});

    protected:
        uint8_t _next_tid();
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

        void set_tready(uint8_t value) { _next_tready = value; }
        uint8_t get_tready() const { return s_axis_tready; }
        void eval_step();

        std::vector<T> read(bool clear = true);
        std::vector<uint8_t> read_tid(bool clear = true);

        static std::shared_ptr<AxisRx<T>> create(
            Signal<uint8_t> &clk,
            Signal<uint8_t> &rst,
            Signal<T> &s_axis_tdata,
            Signal<uint8_t> &s_axis_tvalid,
            Signal<uint8_t> &s_axis_tready,
            Signal<uint8_t> *s_axis_tid = nullptr);

    protected:
        std::deque<T> rx_buf;
        std::deque<uint8_t> tid_buf;
        uint8_t _next_tready = 0;
    };
} // namespace dspsim
