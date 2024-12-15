#pragma once
#include "dspsim/signal.h"
#include <deque>
#include <tuple>
#include <map>
#include <ranges>

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
        void clear()
        {
            _cmd_buf.clear();
            _ack_buf.clear();
        }
        bool busy() const { return cyc_o; }

        // Append a single write command to the buffer.
        void write(AT address, DT data) { command(true, address, data); }
        void writei(AT address, DT data) { write(address, data); }

        // Append to the tx buffer, incrementing the address for each entry, starting at start address.
        template <typename Iter>
        void write(AT start_address, Iter begin, Iter end)
        {
            for (auto &d : std::ranges::subrange(begin, end))
            {
                write(start_address++, d);
            }
        }
        void write(AT start_address, std::list<DT> &data) { write(start_address, data.begin(), data.end()); }
        void writel(AT start_address, std::list<DT> &data) { write(start_address, data); }

        // Write a map/dict of addresses and data.
        void write(std::map<AT, DT> &data)
        {
            for (auto &[address, value] : data)
            {
                write(address, value);
            }
        }
        void writem(std::map<AT, DT> &data) { write(data); }

        // Command to read a sequence of addresses starting with start address and incrementing.
        void read_command(AT start_address, size_t n = 1)
        {
            for (size_t i = 0; i < n; i++)
            {
                command(false, start_address + i);
            }
        }
        void read_commandn(AT start_address, size_t n) { read_command(start_address, n); }

        // Read a set of addresses
        void read_command(std::list<AT> &addresses)
        {
            for (auto &a : addresses)
            {
                read_command(a);
            }
        }
        void read_commandl(std::list<AT> &addresses) { read_command(addresses); }

        // Read out the rx buffer.
        std::vector<DT> rx_data(bool clear = true)
        {
            auto result = std::vector<DT>(_rx_buf.begin(), _rx_buf.end());
            if (clear)
            {
                _rx_buf.clear();
            }
            return result;
        }
        size_t rx_size() const { return _rx_buf.size(); }

        // Send a write command and wait until it's done.
        void write_blocks(AT address, DT data, int timeout = 1000);
        void write_blockl(AT start_address, std::list<DT> &data, int timeout = 10000);
        void write_blockm(std::map<AT, DT> &data, int timeout = 10000);

        // Send a read command and wait for a response. Advances the context sim automatically.
        DT read_blocks(AT address, int timeout = 1000);
        //
        std::vector<DT> read_blockn(AT start_address, size_t n, int timeout = 10000);
        std::vector<DT> read_blockl(std::list<AT> &addresses, int timeout = 10000);

        DT _getitem(AT address) { return read_blocks(address, 1000); }
        void _setitem(AT address, DT data) { write_blocks(address, data, 1000); }
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
        std::deque<std::tuple<AT, DT, bool>> _cmd_buf;
        std::deque<bool> _ack_buf;
        std::deque<DT> _rx_buf;
        bool _transaction_active = false;
    };
} // namespace dspsim
