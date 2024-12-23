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
        Signal<uint8_t> *m_axis_tlast = nullptr;
        std::list<uint8_t> tid_pattern = {0};

    public:
        AxisTx(Signal<uint8_t> &clk,
               Signal<uint8_t> &rst,
               Signal<T> &m_axis_tdata,
               Signal<uint8_t> &m_axis_tvalid,
               Signal<uint8_t> &m_axis_tready,
               Signal<uint8_t> *m_axis_tid = nullptr,
               Signal<uint8_t> *m_axis_tlast = nullptr,
               std::list<uint8_t> tid_pattern = std::list<uint8_t>{0});

        void eval_step();

        bool busy() const { return !buf.empty() || (m_axis_tvalid && m_axis_tready); }

        void write_command(T data);
        void write_command(std::vector<T> &data);
        void writef_command(double data, int q = 0);
        void writef_command(std::vector<double> &data, int q = 0);

        void write_block(T data, int timeout = 1000);
        void write_block(std::vector<T> &data, int timeout = 10000);
        void writef_block(double data, int q = 0, int timeout = 1000);
        void writef_block(std::vector<double> &data, int q = 0, int timeout = 10000);

        int block_wait(int timeout = 1000) const;

        // void write_frame_command(T data);
        // void write_frame_command(std::vector<T> &data);
        // void writef_frame_command(double data, int q = 0);
        // void writef_frame_command(std::vector<double> &data, int q = 0);

        // void write_frame_block(T data, int timeout = 1000);
        // void write_frame_block(std::vector<T> &data, int timeout = 10000);
        // void writef_frame_block(double data, int q = 0, int timeout = 1000);
        // void writef_frame_block(std::vector<double> &data, int q = 0, int timeout = 10000);

        // Create as shared_ptr.
        static std::shared_ptr<AxisTx> create(
            Signal<uint8_t> &clk,
            Signal<uint8_t> &rst,
            Signal<T> &m_axis_tdata,
            Signal<uint8_t> &m_axis_tvalid,
            Signal<uint8_t> &m_axis_tready,
            Signal<uint8_t> *m_axis_tid = nullptr,
            Signal<uint8_t> *m_axis_tlast = nullptr,
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

        std::vector<T> read_rx_buf(bool clear = true);
        std::vector<uint8_t> read_tid(bool clear = true);

        T read_block(int timeout = 1000);
        std::vector<T> read_block(int n, int timeout = 10000);

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
