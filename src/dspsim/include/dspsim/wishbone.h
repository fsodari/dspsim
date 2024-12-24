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
        void clear();
        bool busy() const { return cyc_o || !_cmd_buf.empty(); }

        // Command to read a sequence of addresses starting with start address and incrementing.
        void read_command(AT start_address, size_t n = 1);
        // Read a list of addresses
        void read_command(std::list<AT> &addresses);

        // Rx buffer size.
        size_t rx_size() const { return _rx_buf.size(); }
        // Read out the rx buffer.
        std::vector<DT> rx_data(bool clear = true);

        // Send a read command and wait for a response. Advances the context sim automatically.
        DT read_block(AT address, int timeout = 1000);
        std::vector<DT> read_block(std::list<AT> &addresses, int timeout = 10000);

        // Append a single write command to the buffer.
        void write_command(int address, int64_t data);
        void write_command(int start_address, std::list<int64_t> &data);
        // Write a map/dict of addresses and data.
        void write_command(std::map<int, int64_t> &data);

        // Send a write command and wait until it's done.
        void write_block(int address, int64_t data, int timeout = 1000);
        void write_block(int start_address, std::list<int64_t> &data, int timeout = 10000);
        void write_block(std::map<int, int64_t> &data, int timeout = 10000);

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
            Signal<DT> &data_i);

    protected:
        std::deque<std::tuple<AT, DT, bool>> _cmd_buf; // Command buffer for reads and writes.
        std::deque<bool> _ack_buf;                     // Track responses.
        std::deque<DT> _rx_buf;                        // Receive data buffer.
    };
} // namespace dspsim
