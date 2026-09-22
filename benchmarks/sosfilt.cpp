#include <dspsim/dspsim.h>
#include <array>
#include <cmath>
#include <chrono>
#include <vector>
#include <iostream>

using namespace dspsim;

class IIR2 : public Module
{
public:
    Input<uint8_t> clk{"clk"};
    Input<double> in{"in"};
    Output<double> out{"out"};

    Signal<double> x0{"x0"}, x1{"x1"}, x2{"x2"};
    Signal<double> y0{"y0"}, y1{"y1"}, y2{"y2"};

    std::array<double, 6> coefs;

    IIR2(ModuleName name) : Module(name)
    {
        DSPSIM_METHOD(&IIR2::calc);
        sensitive << clk.pos();

        dont_initialize();
    };

    void calc()
    {
        if (clk.posedge())
        {
            // Input delay line
            x0.write(in.read());
            x1.write(x0.read());
            x2.write(x1.read());

            // Output delay line
            y1.write(y0.read());
            y2.write(y1.read());

            y0.write(coefs[0] * x0.read() + coefs[1] * x1.read() + coefs[2] * x2.read() - coefs[4] * y1.read() - coefs[5] * y2.read());
        }
    }

    void set_coefs(const std::array<double, 6> &c)
    {
        coefs = c;
    }
};

class SOS : public Module
{
public:
    Input<uint8_t> clk{"clk"};
    Input<double> in{"in"};
    Output<double> out{"out"};

    IIR2 iir0{"iir0"};
    IIR2 iir1{"iir1"};
    IIR2 iir2{"iir2"};
    IIR2 iir3{"iir3"};

    Signal<double> o0{"o0"};
    Signal<double> o1{"o1"};
    Signal<double> o2{"o2"};

    SOS(ModuleName name) : Module(name)
    {
        iir0.clk.bind(clk);
        iir0.in.bind(in);
        iir0.out.bind(o0);

        iir1.clk.bind(clk);
        iir1.in.bind(o0);
        iir1.out.bind(o1);

        iir2.clk.bind(clk);
        iir2.in.bind(o1);
        iir2.out.bind(o2);

        iir3.clk.bind(clk);
        iir3.in.bind(o2);
        iir3.out.bind(out);
    };
};

class Top : public Module
{
public:
    Input<uint8_t> clk{"clk"};
    Signal<double> in{"in"};
    Signal<double> out{"out"};

    SOS sos{"sos"};

    std::vector<double> data;
    std::vector<double> out_data;
    size_t iter = 0;

    Top(ModuleName name, size_t data_size) : Module(name), data(data_size)
    {
        sos.iir0.coefs = {2.395964410377617e-05, 4.791928820755234e-05, 2.395964410377617e-05, 1.0, -1.0263514742610553, 0.26864019099379005};
        sos.iir1.coefs = {1.0, 2.0, 1.0, 1.0, -1.0868584613628944, 0.343430940165366};
        sos.iir2.coefs = {1.0, 2.0, 1.0, 1.0, -1.2197253651240232, 0.5076634651740437};
        sos.iir3.coefs = {1.0, 2.0, 1.0, 1.0, -1.4515795942478362, 0.794251053241888};

        sos.clk.bind(clk);
        sos.in.bind(in);
        sos.out.bind(out);

        DSPSIM_METHOD(&Top::eval);
        sensitive << clk.pos();

        // Initialize data vector with a sine wave of 500hz
        double fs = 1e3;
        double f = 500;
        for (size_t i = 0; i < data.size(); ++i)
        {
            data[i] = sin(2 * M_PI * f * i / fs);
        }

        dont_initialize();
    }

    void eval()
    {
        if (clk.posedge())
        {
            if (iter < data.size())
            {
                in.write(data[iter]);
                // out_data[iter] = out.read();
                out_data.push_back(out.read());
                ++iter;
            }
        }
    }
};

int main(int argc, char *argv[])
{
    auto ctx = Context::create();
    // ctx->set_log_level("trace");

    size_t N = 1000000;
    Clock clk{"clk", 10};
    Top top{"top", N};
    Top top2{"top2", N};
    Top top3{"top3", N};
    Top top4{"top4", N};
    top.clk.bind(clk);
    top2.clk.bind(clk);
    top3.clk.bind(clk);
    top4.clk.bind(clk);

    ctx->elaborate();
    ctx->print_hierarchy();

    auto start_time = std::chrono::high_resolution_clock::now();
    ctx->run(N * 10);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Simulation time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Output data size: " << top.out_data.size() << std::endl;

    /*
        With original single threaded eval loop: 2.92027
    */

    return 0;
}