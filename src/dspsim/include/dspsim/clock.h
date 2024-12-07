#pragma once
#include <dspsim/signal.h>
#include <iostream>

namespace dspsim
{
    class Clock : public Signal<uint8_t>
    {
    public:
        Clock(double period);

        void eval_step();

        int period() const
        {
            return m_half_period * 2;
        }

    protected:
        int m_half_period;
        int m_checkpoint;
    };
    using ClockPtr = std::shared_ptr<Clock>;
} // namespace dspsim
