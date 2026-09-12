#include <dspsim/clock.h>

namespace dspsim
{
    Clock::Clock(int period) : Signal<uint8_t>(1), _period(period)
    {
        this->_kind = "clock";
        _half_period = _period / 2;
        _checkpoint = context()->time() + _half_period - 1;
    }

    void Clock::eval_step()
    {
        if (context()->time() >= _checkpoint)
        {
            this->set_d(!this->q());
            _checkpoint += _half_period;
        }
    }
} // namespace dspsim