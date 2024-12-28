#include "dspsim/wishbone.h"
#include <cmath>

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
                _ack_buf.push_back(we_o);
                stb_o = 0;
                we_o = 0;
            }

            // Can send more data.
            if (!_cmd_buf.empty())
            {
                auto [addr_cmd, data, we_cmd] = _cmd_buf.front();
                _cmd_buf.pop_front();
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
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::clear(int amount)
    {
        // amount = (amount < 0 || amount > _ack_buf.size()) ? _ack_buf.size() : amount;
        // _ack_buf.erase(_ack_buf.begin(), _ack_buf.begin() + amount);

        // amount = (amount < 0 || amount > _cmd_buf.size()) ? _cmd_buf.size() : amount;
        // _cmd_buf.erase(_cmd_buf.begin(), _cmd_buf.begin() + amount);

        amount = (amount < 0 || amount > _rx_buf.size()) ? _rx_buf.size() : amount;
        _rx_buf.erase(_rx_buf.begin(), _rx_buf.begin() + amount);
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
    void WishboneM<AT, DT>::read_command(std::vector<AT> &addresses)
    {
        for (auto &a : addresses)
        {
            read_command(a);
        }
    }
    template <typename AT, typename DT>
    std::vector<DT> WishboneM<AT, DT>::rx_data(int amount)
    {
        amount = (amount < 0 || amount > _rx_buf.size()) ? _rx_buf.size() : amount;

        auto result = std::vector<DT>(_rx_buf.begin(), _rx_buf.begin() + amount);
        clear(amount);
        return result;
    }

    template <typename AT, typename DT>
    std::vector<double> WishboneM<AT, DT>::rx_dataf(int q, int amount)
    {
        amount = (amount < 0 || amount > _rx_buf.size()) ? _rx_buf.size() : amount;
        std::vector<double> result;
        result.reserve(amount);
        double sf = std::pow(2, -q);

        std::transform(_rx_buf.begin(), _rx_buf.begin() + amount, std::back_inserter(result), [&sf](const DT &x)
                       { return x * sf; });

        clear(amount);
        return result;
    }

    template <typename AT, typename DT>
    int WishboneM<AT, DT>::wait_block(int n, int timeout)
    {
        for (int i = timeout; i != 0; --i)
        {
            // Advance the simulation
            context()->run(1);
            // Once the simulation is not busy and we have rx_data, read out the buffer and return.
            if (!busy())
            {
                return 1;
            }
        }

        return 0;
    }

    // Send a read command and wait for a response. Advances the context sim automatically.
    template <typename AT, typename DT>
    DT WishboneM<AT, DT>::read_block(AT address, int timeout)
    {
        read_command(address);

        if (wait_block(1, timeout))
        {
            return rx_data(1)[0];
        }

        // Raise exception?
        return 0;
    }

    template <typename AT, typename DT>
    std::vector<DT> WishboneM<AT, DT>::read_block(std::vector<AT> &addresses, int timeout)
    {
        size_t n_expected = addresses.size();

        read_command(addresses);

        if (wait_block(n_expected, timeout))
        {
            return rx_data(n_expected);
        }
        else
        {
            return rx_data(rx_size());
        }
    }

    template <typename AT, typename DT>
    double WishboneM<AT, DT>::readf_block(AT address, int q, int timeout)
    {
        return read_block(address, timeout) * std::pow(2, -q);
    }

    template <typename AT, typename DT>
    std::vector<double> WishboneM<AT, DT>::readf_block(std::vector<AT> &addresses, int q, int timeout)
    {
        size_t n_expected = addresses.size();
        read_command(addresses);
        if (wait_block(n_expected, timeout))
        {
            return rx_dataf(q, n_expected);
        }
        else
        {
            return rx_dataf(q, rx_size());
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_command(AT address, DT data)
    {
        command(true, address, data);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_command(AT start_address, std::vector<DT> &data)
    {
        for (auto &d : data)
        {
            write_command(start_address++, d);
        }
    }
    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_command(std::map<AT, DT> &data)
    {
        for (auto &[address, value] : data)
        {
            write_command(address, value);
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::writef_command(AT address, double data, int q)
    {
        write_command(address, data * std::pow(2, q));
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::writef_command(AT start_address, std::vector<double> &data, int q)
    {
        for (const auto &x : data)
        {
            writef_command(start_address++, x, q);
        }
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::writef_command(std::map<AT, double> &data, int q)
    {
        for (const auto &[k, v] : data)
        {
            writef_command(k, v, q);
        }
    }

    // Send a write command and wait until it's done.
    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_block(AT address, DT data, int timeout)
    {
        write_command(address, data);
        wait_block(1, timeout);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_block(AT start_address, std::vector<DT> &data, int timeout)
    {
        write_command(start_address, data);
        wait_block(data.size(), timeout);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::write_block(std::map<AT, DT> &data, int timeout)
    {
        write_command(data);
        wait_block(data.size(), timeout);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::writef_block(AT address, double data, int q, int timeout)
    {
        writef_command(address, data, q);
        wait_block(1, timeout);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::writef_block(AT start_address, std::vector<double> &data, int q, int timeout)
    {
        // writef_command(start_address)
        for (const auto &d : data)
        {
            writef_command(start_address++, d, q);
        }
        wait_block(data.size(), timeout);
    }

    template <typename AT, typename DT>
    void WishboneM<AT, DT>::writef_block(std::map<AT, double> &data, int q, int timeout)
    {
        for (const auto &[k, v] : data)
        {
            writef_command(k, v, q);
        }
        wait_block(data.size(), timeout);
    }

    // template class WishboneM<uint32_t, int8_t>;
    // template class WishboneM<uint32_t, int16_t>;
    // template class WishboneM<uint32_t, int32_t>;
    // template class WishboneM<uint32_t, int64_t>;
    template class WishboneM<uint32_t, uint8_t>;
    template class WishboneM<uint32_t, uint16_t>;
    template class WishboneM<uint32_t, uint32_t>;
    template class WishboneM<uint32_t, uint64_t>;

} // namespace dspsim
