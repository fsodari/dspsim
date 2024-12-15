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
            : cyc(create<Signal<uint8_t>>()),
              stb(create<Signal<uint8_t>>()),
              we(create<Signal<uint8_t>>()),
              ack(create<Signal<uint8_t>>()),
              stall(create<Signal<uint8_t>>()),
              addr(create<Signal<AT>>()),
              data_o(create<Signal<DT>>()),
              data_i(create<Signal<DT>>())
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
        bool busy() const { return cyc_o; }

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
        void write(AT address, DT data);
        void write(AT start_address, std::list<DT> &data);
        // Write a map/dict of addresses and data.
        void write(std::map<AT, DT> &data);

        // Send a write command and wait until it's done.
        void write_block(AT address, DT data, int timeout = 1000);
        void write_block(AT start_address, std::list<DT> &data, int timeout = 10000);
        void write_block(std::map<AT, DT> &data, int timeout = 10000);

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
