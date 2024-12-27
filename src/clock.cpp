#include "dspsim/clock.h"

namespace dspsim
{
    Clock::Clock(double period) : Signal<uint8_t>(1)
    {
        m_period = period / context()->time_unit();
        m_half_period = m_period / 2;

        m_checkpoint = context()->time() + m_half_period - 1;
    }

    void Clock::eval_step()
    {
        if (this->context()->time() >= m_checkpoint)
        {
            write(!q);
            m_checkpoint += m_half_period;
        }
    }
} // namespace dspsim
