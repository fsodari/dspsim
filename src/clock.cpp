#include <dspsim/clock.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, uint64_t period)
        : Signal<uint8_t>(name, 1, 0, false), _period(period)
    {
        this->_kind = "clock";
        // TODO: Handle non-integer periods.
        _half_period = _period / 2;
        // context()->register_method(&Clock::tick, this, this->hier_name() + ".tick()");
        // _process = context()->register_coro_task(tick_task(), this->hier_name() + ".tick()");
        _process = DSPSIM_CORO(tick_task);
    }

    void Clock::tick()
    {
        // Toggle the clock.
        this->write(!this->_q);

        SPDLOG_LOGGER_TRACE(context()->logger, "{}.tick() scheduled for, time: {}", this->hier_name(), context()->time() + _half_period);

        context()->schedule_time_delta_event(_half_period);
    }

    Task Clock::tick_task()
    {
        while (true)
        {
            this->write(!this->read());
            co_await context()->wait(_half_period, _process);
            this->write(!this->read());
            co_await context()->wait(_half_period, _process);
        }
    }

    uint64_t Clock::period() const
    {
        return _period;
    }
} // namespace dspsim