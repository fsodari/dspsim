#pragma once
#include "dspsim/signal.h"
#include <deque>
#include <tuple>
#include <map>

namespace dspsim
{
    template <typename AT, typename DT>
    struct Wishbone
    {
        SignalPtr<uint8_t> cyc;
        SignalPtr<uint8_t> stb;
        SignalPtr<uint8_t> we;
        SignalPtr<uint8_t> ack;
        SignalPtr<uint8_t> stall;
        SignalPtr<AT> addr;
        SignalPtr<DT> data_o;
        SignalPtr<DT> data_i;

        Wishbone()
            : cyc(Signal<uint8_t>::create()),
              stb(Signal<uint8_t>::create()),
              we(Signal<uint8_t>::create()),
              ack(Signal<uint8_t>::create()),
              stall(Signal<uint8_t>::create()),
              addr(Signal<AT>::create()),
              data_o(Signal<DT>::create()),
              data_i(Signal<DT>::create())
        {
        }
    };

    template <typename AT, typename DT>
    class WishboneM : public Model
    {
    protected:
        Signal<uint8_t> &clk;
        Signal<uint8_t> &rst;
        Signal<uint8_t> &cyc_o;
        Signal<uint8_t> &stb_o;
        Signal<uint8_t> &we_o;
        Signal<uint8_t> &ack_i;
        Signal<uint8_t> &stall_i;
        Signal<AT> &addr_o;
        Signal<DT> &data_o;
        Signal<DT> &data_i;

    public:
        WishboneM(
            Signal<uint8_t> &clk,
            Signal<uint8_t> &rst,
            Signal<uint8_t> &cyc_o,
            Signal<uint8_t> &stb_o,
            Signal<uint8_t> &we_o,
            Signal<uint8_t> &ack_i,
            Signal<uint8_t> &stall_i,
            Signal<AT> &addr_o,
            Signal<DT> &data_o,
            Signal<DT> &data_i);

        void eval_step();

        // Send a command to the interface. If it's a read command, data is ignored.
        void command(bool mode, AT address, DT data = 0);
        void clear(int amount = -1);
        bool busy() const { return cyc_o || !_cmd_buf.empty(); }

        // Command to read a sequence of addresses starting with start address and incrementing.
        void read_command(AT start_address, size_t n = 1);
        // Read a list of addresses
        void read_command(std::vector<AT> &addresses);

        // Rx buffer size.
        size_t rx_size() const { return _rx_buf.size(); }
        // Read out the rx buffer.
        std::vector<DT> rx_data(int amount = -1);
        std::vector<double> rx_dataf(int q, int amount = -1);

        int wait_block(int n, int timeout = -1);

        // Send a read command and wait for a response. Advances the context sim automatically.
        DT read_block(AT address, int timeout = -1);
        std::vector<DT> read_block(std::vector<AT> &addresses, int timeout = -1);

        double readf_block(AT address, int q, int timeout = -1);
        std::vector<double> readf_block(std::vector<AT> &addresses, int q, int timeout = -1);

        // Append a single write command to the buffer.
        void write_command(AT address, DT data);
        void write_command(AT start_address, std::vector<DT> &data);
        // Write a map/dict of addresses and data.
        void write_command(std::map<AT, DT> &data);

        void writef_command(AT address, double data, int q);
        void writef_command(AT start_address, std::vector<double> &data, int q);
        void writef_command(std::map<AT, double> &data, int q);

        // Send a write command and wait until it's done.
        void write_block(AT address, DT data, int timeout = -1);
        void write_block(AT start_address, std::vector<DT> &data, int timeout = -1);
        void write_block(std::map<AT, DT> &data, int timeout = -1);
        void writef_block(AT address, double data, int q, int timeout = -1);
        void writef_block(AT start_address, std::vector<double> &data, int q, int timeout = -1);
        void writef_block(std::map<AT, double> &data, int q, int timeout = -1);

        static std::shared_ptr<WishboneM<AT, DT>> create(
            Signal<uint8_t> &clk,
            Signal<uint8_t> &rst,
            Signal<uint8_t> &cyc_o,
            Signal<uint8_t> &stb_o,
            Signal<uint8_t> &we_o,
            Signal<uint8_t> &ack_i,
            Signal<uint8_t> &stall_i,
            Signal<AT> &addr_o,
            Signal<DT> &data_o,
            Signal<DT> &data_i)
        {
            return Model::create<WishboneM<AT, DT>>(clk, rst, cyc_o, stb_o, we_o, ack_i, stall_i, addr_o, data_o, data_i);
        }

    protected:
        std::deque<std::tuple<AT, DT, bool>> _cmd_buf; // Command buffer for reads and writes.
        std::deque<bool> _ack_buf;                     // Track responses.

        std::deque<DT> _rx_buf; // Receive data buffer.
    };
} // namespace dspsim
