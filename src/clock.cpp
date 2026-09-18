#include <dspsim/clock.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, int period)
        : Signal<uint8_t>(name, 1, 0, false), _period(period)
    {
        _half_period = _period / 2;
        context()->add_to_eval_queue(this);
        // context()->add_to_time_event_queue(TimeEvent(this, context()->time() + _half_period));
    }

    void Clock::eval()
    {
        this->_d = !this->_q;
        context()->add_to_time_event_queue(TimeEvent(this, context()->time() + _half_period));
        context()->add_to_update_queue(this);
    }
} // namespace dspsim