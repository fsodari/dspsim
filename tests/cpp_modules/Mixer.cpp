#include "Mixer.h"

#ifndef MIXER_GAIN
#error "MIXER_GAIN is not defined"
#endif

namespace demo
{
    Mixer::Mixer(dspsim::ModuleName name) : Mixer(name, 0)
    {
    }

    Mixer::Mixer(dspsim::ModuleName, int offset) : offset_(offset)
    {
        DSPSIM_CORO(eval);
    }

    int Mixer::gain() const
    {
        return MIXER_GAIN;
    }

    int Mixer::offset() const
    {
        return offset_;
    }

    void Mixer::set_offset(int offset)
    {
        offset_ = offset;
    }

    std::vector<int> Mixer::history() const
    {
        return history_;
    }

    dspsim::Task<> Mixer::eval()
    {
        while (true)
        {
            co_await wait(clk.pos());
            const mixer_sum_t value = a.read() * MIXER_GAIN + b.read() + offset_;
            history_.push_back(static_cast<int>(value));
            y.write(value);
        }
    }

    Passive::Passive(dspsim::ModuleName)
    {
    }

    int Passive::eval() const
    {
        return 7;
    }
} // namespace demo
