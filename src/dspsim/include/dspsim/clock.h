#pragma once
#include <dspsim/signal.h>
#include <iostream>

namespace dspsim
{
    class Clock : public Signal<uint8_t>
    {
    public:
        Clock(double period) : Signal<uint8_t>(1)
        {
            int period_ratio = period / context()->time_unit();
            m_half_period = period_ratio / 2;
            std::cout << "Clock: " << m_half_period << std::endl;

            m_checkpoint = context()->time() + m_half_period - 1;
        }

        void eval_step()
        {
            if (this->context()->time() >= m_checkpoint)
            {
                this->d = !this->q;
                m_checkpoint += m_half_period;
            }
        }

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
