#include <dspsim/dspsim.h>
#include <dspsim/dff.h>
#include <spdlog/spdlog.h>
#include <dspsim/vmodel.h>

#include <VSkid.h>
#include <verilated_vcd_c.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

template <typename V, typename TraceType>
class VX : public VModuleBase
{
public:
    std::unique_ptr<VerilatedContext> vcontext = std::make_unique<VerilatedContext>();
    std::unique_ptr<V> top = std::make_unique<V>(vcontext.get());
    std::unique_ptr<VerilatedVcdC> tracep;

    VX(ModuleName &name) : VModuleBase(name)
    {
        context()->logger->info("Initializing VX module: {}", this->name());
    }

    void _sync_outputs()
    {
        for (auto output : _outputs)
        {
            output->_notify(EventType::Changed);
        }
    }

    void eval() override
    {
        top->eval_step();
    }

    void update() override
    {
        top->eval_end_step();
        this->_sync_outputs();
    }

    void dump_trace() override
    {
        if (tracep)
        {
            tracep->dump(context()->time());
        }
    }

    void trace(const fs::path &trace_path, int levels = 99, int options = 0)
    {
        if (!tracep)
        {
            vcontext->traceEverOn(true);
            tracep = std::make_unique<TraceType>();

            tracep->set_time_unit(context()->time_unit().c_str());
            tracep->set_time_resolution(context()->time_unit().c_str());

            top->trace(tracep.get(), levels, options);
            tracep->open(trace_path.string().c_str());
        }
    }

    void close()
    {
        if (tracep)
        {
            tracep->close();
        }
    }
};

#define DSPSIM_VINPUT(name) \
    VInput<std::remove_reference_t<decltype(top->name)>> name { #name, top->name }

#define DSPSIM_VOUTPUT(name) \
    VOutput<std::remove_reference_t<decltype(top->name)>> name { #name, top->name }

#define DSPSIM_VCTOR(_name, trace_type) \
    _name(ModuleName name) : BaseT(name) {}

class Skid : public VX<VSkid, VerilatedVcdC>
{
    using BaseT = VX<VSkid, VerilatedVcdC>;

public:
    DSPSIM_VINPUT(clk);
    DSPSIM_VINPUT(rst);
    DSPSIM_VINPUT(s_axis_tdata);
    DSPSIM_VINPUT(s_axis_tvalid);
    DSPSIM_VOUTPUT(s_axis_tready);
    DSPSIM_VOUTPUT(m_axis_tdata);
    DSPSIM_VOUTPUT(m_axis_tvalid);
    DSPSIM_VINPUT(m_axis_tready);

    DSPSIM_VCTOR(Skid, VerilatedVcdC);
};

// class Skid : public VModel<VSkid, VerilatedVcdC>
// {
//     using VBase = VModel<VSkid, VerilatedVcdC>;

// public:
//     VInput<uint8_t> clk{"clk"};
//     VInput<uint8_t> rst{"rst"};
//     VInput<uint32_t> s_axis_tdata{"s_axis_tdata"};
//     VInput<uint8_t> s_axis_tvalid{"s_axis_tvalid"};
//     VOutput<uint8_t> s_axis_tready{"s_axis_tready"};
//     VOutput<uint32_t> m_axis_tdata{"m_axis_tdata"};
//     VOutput<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
//     VInput<uint8_t> m_axis_tready{"m_axis_tready"};

//     Skid(ModuleName name) : VBase(name)
//     {
//         clk.bind_ext_port(top->clk);
//         rst.bind_ext_port(top->rst);
//         s_axis_tdata.bind_ext_port(top->s_axis_tdata);
//         s_axis_tvalid.bind_ext_port(top->s_axis_tvalid);
//         s_axis_tready.bind_ext_port(top->s_axis_tready);
//         m_axis_tdata.bind_ext_port(top->m_axis_tdata);
//         m_axis_tvalid.bind_ext_port(top->m_axis_tvalid);
//         m_axis_tready.bind_ext_port(top->m_axis_tready);
//     }
// };

template <typename T>
class AxisTx : public Module
{
public:
    Input<uint8_t> clk{"clk"};
    Input<uint8_t> rst{"rst"};
    Output<T> m_axis_tdata{"m_axis_tdata"};
    Output<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
    Input<uint8_t> m_axis_tready{"m_axis_tready"};

    std::deque<T> fifo;

    AxisTx(ModuleName name) : Module(name)
    {
        always << clk.pos();
    }

    void send(T &data)
    {
        fifo.push_back(data);
    }

    void send(std::vector<T> &data)
    {
        fifo.insert(fifo.end(), data.begin(), data.end());
    }

    template <typename InputIt>
    void send(InputIt first, InputIt last)
    {
        fifo.insert(fifo.end(), first, last);
    }

    void send(std::initializer_list<T> data)
    {
        fifo.insert(fifo.end(), data.begin(), data.end());
    }

    void eval() override
    {
        if (clk.posedge())
        {
            this->context()->logger->debug("AxisTx eval at posedge clk");
            // A valid transaction has occurred, pop the front of the FIFO.
            if (m_axis_tvalid.read() && m_axis_tready.read())
            {
                // Clear the valid signal as the transaction has been accepted.
                m_axis_tvalid.write(0);
                fifo.pop_front();
            }

            // Bus is waiting for downstream to be ready.
            if (!m_axis_tready.read() && m_axis_tvalid.read())
            {
            }
            // We have data to send.
            else if (!fifo.empty())
            {
                m_axis_tvalid.write(1);
                m_axis_tdata.write(fifo.front());
            }
            else
            {
                // Can we get here?
                m_axis_tvalid.write(0);
            }

            if (rst.read() == 1)
            {
                m_axis_tvalid.write(0);
            }
        }
    }
};

template <typename T>
class AxisRx : public Module
{
public:
    Input<uint8_t> clk{"clk"};
    Input<uint8_t> rst{"rst"};
    Input<T> s_axis_tdata{"s_axis_tdata"};
    Input<uint8_t> s_axis_tvalid{"s_axis_tvalid"};
    Output<uint8_t> s_axis_tready{"s_axis_tready"};

    std::deque<T> fifo;
    uint8_t _ready = 0;

    AxisRx(ModuleName name) : Module(name)
    {
        always << clk.pos();
    }

    void eval() override
    {
        if (clk.posedge())
        {
            if (s_axis_tvalid.read() && s_axis_tready.read())
            {
                fifo.push_back(s_axis_tdata.read());
            }

            // No limit to buffer size or throughput, so set ready when user requests.
            s_axis_tready.write(_ready);
        }
    }

    void ready(uint8_t r)
    {
        _ready = r;
    }
    uint8_t ready() const
    {
        return _ready;
    }
};

TEST_CASE("test_vmodel", "[vmodel]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::trace);

    Clock clk{"clk", 10};

    Signal<uint8_t> rst{"rst"};
    Signal<uint32_t> s_axis_tdata{"s_axis_tdata"};
    Signal<uint8_t> s_axis_tvalid{"s_axis_tvalid"};
    Signal<uint8_t> s_axis_tready{"s_axis_tready"};
    Signal<uint32_t> m_axis_tdata{"m_axis_tdata"};
    Signal<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
    Signal<uint8_t> m_axis_tready{"m_axis_tready"};

    Skid skid{"skid"};
    skid.trace("traces/skid.vcd");

    AxisTx<uint32_t> axis_tx{"axis_tx"};
    AxisRx<uint32_t> axis_rx{"axis_rx"};

    skid.clk.bind(clk);
    skid.rst.bind(rst);
    skid.s_axis_tdata.bind(s_axis_tdata);
    skid.s_axis_tvalid.bind(s_axis_tvalid);
    skid.s_axis_tready.bind(s_axis_tready);
    skid.m_axis_tdata.bind(m_axis_tdata);
    skid.m_axis_tvalid.bind(m_axis_tvalid);
    skid.m_axis_tready.bind(m_axis_tready);

    axis_tx.clk.bind(clk);
    axis_tx.rst.bind(rst);
    axis_tx.m_axis_tdata.bind(s_axis_tdata);
    axis_tx.m_axis_tvalid.bind(s_axis_tvalid);
    axis_tx.m_axis_tready.bind(s_axis_tready);

    axis_rx.clk.bind(clk);
    axis_rx.rst.bind(rst);
    axis_rx.s_axis_tdata.bind(m_axis_tdata);
    axis_rx.s_axis_tvalid.bind(m_axis_tvalid);
    axis_rx.s_axis_tready.bind(m_axis_tready);

    ctx->elaborate();
    ctx->run(100);
    auto tx_data = std::vector<uint32_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    axis_tx.send(tx_data);
    ctx->run(100);
    axis_rx.ready(1);
    ctx->run(50);
    axis_rx.ready(0);
    ctx->run(50);
    axis_rx.ready(1);
    ctx->run(100);

    auto rx_data = std::vector<uint32_t>(axis_rx.fifo.begin(), axis_rx.fifo.end());
    REQUIRE(rx_data == tx_data);

    // REQUIRE(s_axis_tready.read() == 1);

    // s_axis_tdata.write(99);
    // s_axis_tvalid.write(1);
    // ctx->run(10);
    // s_axis_tvalid.write(0);
    // ctx->run(10);
    // // Can hold 2
    // REQUIRE(s_axis_tready.read() == 1);

    // s_axis_tdata.write(100);
    // s_axis_tvalid.write(1);
    // ctx->run(10);
    // s_axis_tvalid.write(0);
    // ctx->run(10);
    // // Full at this point.
    // REQUIRE(s_axis_tready.read() == 0);

    // REQUIRE(m_axis_tdata.read() == 99);
    // REQUIRE(m_axis_tvalid.read() == 1);
    // m_axis_tready.write(1);
    // ctx->run(10);
    // m_axis_tready.write(0);
    // ctx->run(10);
    // REQUIRE(m_axis_tdata.read() == 100);
    // REQUIRE(m_axis_tvalid.read() == 1);
    // m_axis_tready.write(1);
    // ctx->run(10);
    // m_axis_tready.write(0);
    // ctx->run(10);

    // REQUIRE(m_axis_tvalid.read() == 0);
}
