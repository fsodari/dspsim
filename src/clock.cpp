#include <dspsim/clock.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, int period)
        : Signal<uint8_t>(name, 1, 0, false), _period(period)
    {
        this->_kind = "clock";
        // TODO: Handle non-integer periods.
        _half_period = _period / 2;
        _process = context()->register_method(&Clock::tick, this, this->name() + ".tick()");
    }

    void Clock::tick()
    {
        this->write(!this->_q);
        SPDLOG_LOGGER_TRACE(context()->logger, "Clock tick scheduled for process: {}, time: {}", _process->name(), context()->time() + _half_period);
    }

    void Clock::update()
    {
        SPDLOG_LOGGER_TRACE(context()->logger, "Clock update for process: {}, time: {}", _process->name(), context()->time() + _half_period);
        Signal<uint8_t>::update();
        context()->_time_event_stack.emplace(context(), _process, context()->time() + _half_period);
    }

    int Clock::period() const
    {
        return _period;
    }
} // namespace dspsim