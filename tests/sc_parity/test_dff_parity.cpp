#include <systemc>
#include <dspsim/dspsim.h>
#include <dspsim/dff.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <iostream>

template <typename T>
SC_MODULE(DffSC)
{
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<T> d;
    sc_core::sc_out<T> q;

    SC_CTOR(DffSC)
    {
        SC_METHOD(eval);
        sensitive << clk.pos();
    }

    void eval()
    {
        if (clk.posedge())
        {
            q.write(d.read());
        }
    }
};

using namespace dspsim;

int sc_main(int argc, char *argv[])
{
    // Create dspsim design.
    auto ctx = Context::create();
    Clock clkd{"clkd", 10};
    Signal<uint32_t> dd{"dd"};
    Signal<uint32_t> qd{"qq"};
    Dff<uint32_t> dffd{"dffd"};
    dffd.clk.bind(clkd);
    dffd.d.bind(dd);
    dffd.q.bind(qd);
    ctx->elaborate();

    // Create SystemC design.
    sc_core::sc_clock clks{"clks", 10, sc_core::SC_NS};
    sc_core::sc_signal<uint32_t> ds{"ds"};
    sc_core::sc_signal<uint32_t> qs{"qs"};
    DffSC<uint32_t> dffsc{"dffsc"};
    dffsc.clk(clks);
    dffsc.d(ds);
    dffsc.q(qs);

    auto run_both = [&](int steps)
    {
        ctx->run(steps);
        sc_core::sc_start(steps, sc_core::SC_NS);
    };

    // Run both simulations at same time.
    run_both(15);
    assert(dd.read() == 0);
    assert(dd.read() == ds.read());
    assert(qd.read() == qs.read());

    for (int i = 1; i < 3; i++)
    {
        dd.write(i);
        ds.write(i);
        run_both(5);
        // d signal will have changed before the clock posedge, but q shouldn't
        assert(qd.read() != dd.read());
        assert(qs.read() != ds.read());

        run_both(5);

        // q should have changed at the clock edge.
        assert(qd.read() == dd.read());
        assert(qs.read() == ds.read());
    }

    return 0;
}
