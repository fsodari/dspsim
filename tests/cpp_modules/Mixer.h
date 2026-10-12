#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

#include <cstdint>
#include <vector>

namespace demo
{
    // MIXER_WIDE selects the type of the output port: the parser has to see the compile definitions.
#ifdef MIXER_WIDE
    using mixer_sum_t = int64_t;
#else
    using mixer_sum_t = int32_t;
#endif

    /// y <= a * MIXER_GAIN + b + offset on every rising clock edge. Declared here and defined in Mixer.cpp.
    class Mixer : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<int16_t> a{"a"};
        dspsim::Input<int16_t> b{"b"};
        dspsim::Output<mixer_sum_t> y{"y"};

        Mixer(dspsim::ModuleName name);
        Mixer(dspsim::ModuleName name, int offset);

        int gain() const;
        int offset() const;
        void set_offset(int offset);
        /// The values written to y so far.
        std::vector<int> history() const;

        /// The process: a constructor in Mixer.cpp registers it.
        dspsim::Task<> eval();

    private:
        int offset_;
        std::vector<int> history_;
    };

    /// Defined in the same source file. Its eval() is not a process, but a method for Python.
    class Passive : public dspsim::Module
    {
    public:
        Passive(dspsim::ModuleName name);

        int eval() const;
    };
} // namespace demo
