#include "dspsim/wishbone.h"

namespace dspsim
{
    template <typename AT, typename DT>
    WishboneM<AT, DT>::WishboneM(
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
        : clk(clk),
          rst(rst),
          cyc_o(cyc_o),
          stb_o(stb_o),
          we_o(we_o),
          ack_i(ack_i),
          stall_i(stall_i),
          addr_o(addr_o),
          data_o(data_o),
          data_i(data_i)
    {
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::eval_step()
    {
        if (clk.posedge())
        {
            // The last command was accepted.
            if (cyc_o && stb_o && !stall_i)
            {
                _cmd_buf.pop_front();
                stb_o = 0;
                we_o = 0;

                if (_ack_buf.empty())
                {
                    cyc_o = 0;
                }
            }

            // Can send more data.
            if (!_cmd_buf.empty())
            {
                auto [addr_cmd, data, we_cmd] = _cmd_buf.front();
                cyc_o = 1;
                stb_o = 1;
                we_o = we_cmd;
                addr_o = addr_cmd;
                data_o = data;
            }

            // Accept data response
            if (cyc_o && ack_i)
            {
                // Only push read command acks into the rx_buf
                auto ack_we_cmd = _ack_buf.front();
                if (!ack_we_cmd)
                {
                    _rx_buf.push_back(data_i);
                }
                _ack_buf.pop_front();

                // Done with a transaction.
                if (_ack_buf.empty())
                {
                    cyc_o = 0;
                }
            }
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::command(bool mode, AT address, DT data)
    {
        _cmd_buf.push_back({address, data, mode});
        _ack_buf.push_back(mode);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::clear()
    {
        _cmd_buf.clear();
        _ack_buf.clear();
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::read_command(AT start_address, size_t n)
    {
        for (size_t i = 0; i < n; i++)
        {
            command(false, start_address + i);
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::read_command(std::list<AT> &addresses)
    {
        for (auto &a : addresses)
        {
            read_command(a);
        }
    }
    template <typename AT, typename DT>
    std::vector<DT> WishboneM<AT, DT>::rx_data(bool clear)
    {
        auto result = std::vector<DT>(_rx_buf.begin(), _rx_buf.end());
        if (clear)
        {
            _rx_buf.clear();
        }
        return result;
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write(AT address, DT data)
    {
        command(true, address, data);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write(AT start_address, std::list<DT> &data)
    {
        for (auto &d : data)
        {
            write(start_address++, d);
        }
    }
    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write(std::map<AT, DT> &data)
    {
        for (auto &[address, value] : data)
        {
            write(address, value);
        }
    }

    // template <typename AT, typename DT>
    // void WishboneM<AT, DT>::
    // template <typename AT, typename DT>
    // void WishboneM<AT, DT>::

    template <typename AT, typename DT>
    std::shared_ptr<WishboneM<AT, DT>> WishboneM<AT, DT>::create(
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
        auto wbm = std::make_shared<WishboneM<AT, DT>>(clk, rst, cyc_o, stb_o, we_o, ack_i, stall_i, addr_o, data_o, data_i);
        wbm->context()->own_model(wbm);
        return wbm;
    }

    // Send a read command and wait for a response. Advances the context sim automatically.
    template <typename AT, typename DT>
    DT WishboneM<AT, DT>::read_block(AT address, int timeout)
    {
        read_command(address);

        for (int i = 0; i < timeout; ++i)
        {
            // Advance the simulation
            context()->advance(1);
            // Once the simulation is not busy and we have rx_data, read out the buffer and return.
            if (!busy() && rx_size() >= 1)
            {
                auto result = rx_data(true);
                return result[0];
            }
        }
        // Raise exception?
        return 0;
    }
    // //
    // template <typename AT, typename DT>
    // std::vector<DT> WishboneM<AT, DT>::read_block(AT start_address, size_t n, int timeout)
    // {
    //     read_command(start_address, n);

    //     for (int i = 0; i < timeout; ++i)
    //     {
    //         // Advance the simulation
    //         context()->advance(1);
    //         // Once the simulation is not busy and we have rx_data, read out the buffer and return.
    //         if (!busy() && rx_size() >= n)
    //         {
    //             return rx_data(true);
    //         }
    //     }
    //     // Raise exception?
    //     return std::vector<DT>();
    // }
    template <typename AT, typename DT>
    std::vector<DT> WishboneM<AT, DT>::read_block(std::list<AT> &addresses, int timeout)
    {
        size_t n_expected = addresses.size();

        read_command(addresses);

        for (int i = 0; i < timeout; ++i)
        {
            // Advance the simulation
            context()->advance(1);
            // Once the simulation is not busy and we have rx_data, read out the buffer and return.
            if (!busy() && rx_size() >= n_expected)
            {
                return rx_data(true);
            }
        }
        // Raise exception?
        return std::vector<DT>();
    }

    // Send a write command and wait until it's done.
    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_block(AT address, DT data, int timeout)
    {
        write(address, data);
        for (int i = 0; i < timeout; ++i)
        {
            context()->advance(1);
            if (!busy())
            {
                return;
            }
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_block(AT start_address, std::list<DT> &data, int timeout)
    {
        write(start_address, data);

        for (int i = 0; i < timeout; ++i)
        {
            context()->advance(1);
            if (!busy())
            {
                return;
            }
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_block(std::map<AT, DT> &data, int timeout)
    {
        write(data);

        for (int i = 0; i < timeout; ++i)
        {
            context()->advance(1);
            if (!busy())
            {
                return;
            }
        }
    }

    template class WishboneM<uint32_t, uint8_t>;
    template class WishboneM<uint32_t, uint16_t>;
    template class WishboneM<uint32_t, uint32_t>;
    template class WishboneM<uint32_t, uint64_t>;

} // namespace dspsim
