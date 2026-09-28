#include <dspsim/clock.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, uint64_t period)
        : Signal<uint8_t>(name, 1, 0, false), _period(period)
    {
        this->_kind = "clock";

        _half_period = _period / 2;
        // If half_period wasn't an even number.
        _remainder = _period - _half_period;
        _process = DSPSIM_CORO(tick);
    }

    Task Clock::tick()
    {
        while (true)
        {
            this->write(!this->read());
            co_await context()->wait(_half_period, _process);
            this->write(!this->read());
            co_await context()->wait(_remainder, _process);
        }
    }

    uint64_t Clock::period() const
    {
        return _period;
    }
} // namespace dspsim