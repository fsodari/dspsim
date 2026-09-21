#include <dspsim/clock.h>
#include <dspsim/context.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, int period)
        : Signal<uint8_t>(name, 1, 0, false), _period(period)
    {
        this->_kind = "clock";
        // TODO: Handle non-integer periods.
        _half_period = _period / 2;
        context()->_push_eval_stack(this);
    }

    void Clock::eval()
    {
        this->write(!this->_q);
        context()->_push_time_event_stack(TimeEvent(this, context()->time() + _half_period));
    }

    int Clock::period() const
    {
        return _period;
    }
} // namespace dspsim