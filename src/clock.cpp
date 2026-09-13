#include <dspsim/clock.h>

namespace dspsim
{
    Clock::Clock(int period, const std::string &name) : Signal<uint8_t>(1, 1, false, name), _period(period), _q_prev(0)
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

    void Clock::eval_end_step()
    {
        this->_q_prev = this->q();
        _sync();
    }
} // namespace dspsim