#pragma once
#include "dspsim/signal.h"
#include <deque>
#include <tuple>
#include <map>

namespace dspsim
{
    template <typename AT, typename DT>
    class AxilM : public Model
    {
    protected:
        Signal8 &clk;
        Signal8 &rst;

        Signal<AT> &m_axil_awaddr;
        Signal8 &m_axil_awvalid;
        Signal8 &m_axil_awready;

        Signal<DT> &m_axil_wdata;
        Signal8 &m_axil_wvalid;
        Signal8 &m_axil_wready;

        Signal8 &m_axil_bresp;
        Signal8 &m_axil_bvalid;
        Signal8 &m_axil_bready;

        Signal<AT> &m_axil_araddr;
        Signal8 &m_axil_arvalid;
        Signal8 &m_axil_arready;

        Signal<DT> &m_axil_rdata;
        Signal8 &m_axil_rresp;
        Signal8 &m_axil_rvalid;
        Signal8 &m_axil_rready;

    public:
        AxilM(Signal8 &clk,
              Signal8 &rst,

              Signal<AT> &m_axil_awaddr,
              Signal8 &m_axil_awvalid,
              Signal8 &m_axil_awready,

              Signal<DT> &m_axil_wdata,
              Signal8 &m_axil_wvalid,
              Signal8 &m_axil_wready,

              Signal8 &m_axil_bresp,
              Signal8 &m_axil_bvalid,
              Signal8 &m_axil_bready,

              Signal<AT> &m_axil_araddr,
              Signal8 &m_axil_arvalid,
              Signal8 &m_axil_arready,

              Signal<DT> &m_axil_rdata,
              Signal8 &m_axil_rresp,
              Signal8 &m_axil_rvalid,
              Signal8 &m_axil_rready)
            : clk(clk), rst(rst),
              m_axil_awaddr(m_axil_awaddr), m_axil_awvalid(m_axil_awvalid), m_axil_awready(m_axil_awready),
              m_axil_wdata(m_axil_wdata), m_axil_wvalid(m_axil_wvalid), m_axil_wready(m_axil_wready),
              m_axil_bresp(m_axil_bresp), m_axil_bvalid(m_axil_bvalid), m_axil_bready(m_axil_bready),
              m_axil_araddr(m_axil_araddr), m_axil_arvalid(m_axil_arvalid), m_axil_arready(m_axil_arready),
              m_axil_rdata(m_axil_rdata), m_axil_rresp(m_axil_rresp), m_axil_rvalid(m_axil_rvalid), m_axil_rready(m_axil_rready)
        {
        }
        static std::shared_ptr<AxilM<AT, DT>> create(
            Signal8 &clk,
            Signal8 &rst,

            Signal<AT> &m_axil_awaddr,
            Signal8 &m_axil_awvalid,
            Signal8 &m_axil_awready,

            Signal<DT> &m_axil_wdata,
            Signal8 &m_axil_wvalid,
            Signal8 &m_axil_wready,

            Signal8 &m_axil_bresp,
            Signal8 &m_axil_bvalid,
            Signal8 &m_axil_bready,

            Signal<AT> &m_axil_araddr,
            Signal8 &m_axil_arvalid,
            Signal8 &m_axil_arready,

            Signal<DT> &m_axil_rdata,
            Signal8 &m_axil_rresp,
            Signal8 &m_axil_rvalid,
            Signal8 &m_axil_rready)
        {
            return Model::create<AxilM<AT, DT>>(
                clk,
                rst,
                m_axil_awaddr,
                m_axil_awvalid,
                m_axil_awready,
                m_axil_wdata,
                m_axil_wvalid,
                m_axil_wready,
                m_axil_bresp,
                m_axil_bvalid,
                m_axil_bready,
                m_axil_araddr,
                m_axil_arvalid,
                m_axil_arready,
                m_axil_rdata,
                m_axil_rresp,
                m_axil_rvalid,
                m_axil_rready);
        }

        void eval_step()
        {
            if (clk.posedge())
            {
                // Always allow responses
                m_axil_bready = _bready;
                m_axil_rready = _rready;

                // Write channel.
                // Output address accepted
                if (m_axil_awvalid && m_axil_awready)
                {
                    m_axil_awvalid = 0;
                }
                // Output data accepted.
                if (m_axil_wvalid && m_axil_wready)
                {
                    m_axil_wvalid = 0;
                }
                // Send write address command
                if (!awaddr_buf.empty() && (!m_axil_awvalid || m_axil_awready))
                {
                    m_axil_awaddr = awaddr_buf.front();
                    awaddr_buf.pop_front();

                    m_axil_awvalid = 1;
                }
                // Send write data command.
                if (!wdata_buf.empty() && (!m_axil_wvalid || m_axil_wready))
                {
                    m_axil_wdata = wdata_buf.front();
                    wdata_buf.pop_front();

                    m_axil_wvalid = 1;
                }
                // Receive write response
                if (m_axil_bvalid && m_axil_bready)
                {
                    bresp_buf.push_back(m_axil_bresp);
                }

                // Read channel
                if (m_axil_arvalid && m_axil_arready)
                {
                    m_axil_arvalid = 0;
                }
                // Read address command.
                if (!araddr_buf.empty() && (!m_axil_arvalid || m_axil_arready))
                {
                    m_axil_araddr = araddr_buf.front();
                    araddr_buf.pop_front();

                    m_axil_arvalid = 1;
                }
                // Read data/resp response
                if (m_axil_rvalid && m_axil_rready)
                {
                    rdata_buf.push_back(m_axil_rdata);
                    rresp_buf.push_back(m_axil_rresp);
                }
            }
        }

        uint8_t get_bready() const { return m_axil_bready; }
        void set_bready(uint8_t bready) { _bready = bready; }
        uint8_t get_rready() const { return m_axil_rready; }
        void set_rready(uint8_t rready) { _rready = rready; }

        //
        void write_addr_command(AT address) { awaddr_buf.push_back(address); }
        void write_data_command(DT data) { wdata_buf.push_back(data); }

        // Send address and data at the same time.
        void write_command(AT address, DT data)
        {
            write_addr_command(address);
            write_data_command(data);
        }
        void write_command(AT start_address, std::vector<DT> &data)
        {
            for (const auto &d : data)
            {
                write_command(start_address++, d);
            }
        }
        void write_command(std::map<AT, DT> &data)
        {
            for (const auto &[a, d] : data)
            {
                write_command(a, d);
            }
        }

        std::vector<uint8_t> read_bresp_buf(int amount = -1)
        {
            amount = (amount < 0 || amount > bresp_buf.size()) ? bresp_buf.size() : amount;

            std::vector<uint8_t> result(bresp_buf.begin(), bresp_buf.begin() + amount);
            bresp_buf.erase(bresp_buf.begin(), bresp_buf.begin() + amount);
            return result;
        }

        int _wait_bresp(int n, int timeout)
        {
            for (int i = timeout; i != 0; --i)
            {
                context()->run(1);
                if (bresp_buf.size() >= n)
                {
                    return 1;
                }
            }
            return 0;
        }

        uint8_t write(AT address, DT data, int timeout = -1)
        {
            write_command(address, data);
            if (_wait_bresp(1, timeout))
            {
                return read_bresp_buf(1)[0];
            }
            else
            {
                // Error code?
                return 0xFF;
            }
        }
        std::vector<uint8_t> write(AT start_address, std::vector<DT> &data, int timeout = -1)
        {
            write_command(start_address, data);
            _wait_bresp(data.size(), timeout);

            return read_bresp_buf(data.size());
        }

        std::vector<uint8_t> write(std::map<AT, DT> &data, int timeout = -1)
        {
            write_command(data);
            _wait_bresp(data.size(), timeout);

            return read_bresp_buf(data.size());
        }

        void read_command(AT address) { araddr_buf.push_back(address); }
        void read_command(std::vector<AT> &addresses)
        {
            for (const auto &a : addresses)
            {
                read_command(a);
            }
        }

        std::vector<DT> read_rdata_buf(int amount = -1)
        {
            amount = (amount < 0 || amount > rdata_buf.size()) ? rdata_buf.size() : amount;

            std::vector<DT> result(rdata_buf.begin(), rdata_buf.begin() + amount);
            rdata_buf.erase(rdata_buf.begin(), rdata_buf.begin() + amount);
            return result;
        }

        std::vector<uint8_t> read_rresp_buf(int amount = -1)
        {
            amount = (amount < 0 || amount > rresp_buf.size()) ? rresp_buf.size() : amount;

            std::vector<uint8_t> result(rresp_buf.begin(), rresp_buf.begin() + amount);
            rresp_buf.erase(rresp_buf.begin(), rresp_buf.begin() + amount);
            return result;
        }

        int _wait_rresp(int n, int timeout)
        {
            for (int i = timeout; i != 0; --i)
            {
                context()->run(1);
                if (rresp_buf.size() >= n)
                {
                    return 1;
                }
            }
            return 0;
        }

        std::tuple<DT, uint8_t> read(AT address, int timeout = -1)
        {
            read_command(address);
            DT rdata = 0;
            uint8_t rresp = 0;

            if (_wait_rresp(1, timeout))
            {
                rdata = read_rdata_buf(1)[0];
                rresp = read_rresp_buf(1)[0];
            }

            return std::make_tuple<>(rdata, rresp);
        }

        std::tuple<std::vector<DT>, std::vector<uint8_t>> read(std::vector<AT> &addresses, int timeout = -1)
        {
            int n = addresses.size();

            read_command(addresses);
            _wait_rresp(n, timeout);

            auto rdata = read_rdata_buf(n);
            auto rresp = read_rresp_buf(n);

            return std::make_tuple<>(rdata, rresp);
        }

    protected:
        std::deque<AT> awaddr_buf;
        std::deque<DT> wdata_buf;
        std::deque<uint8_t> bresp_buf;
        std::deque<AT> araddr_buf;
        std::deque<DT> rdata_buf;
        std::deque<uint8_t> rresp_buf;
        uint8_t _bready = 1, _rready = 1;
    };
}
