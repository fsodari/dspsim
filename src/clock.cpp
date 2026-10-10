#include <dspsim/clock.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, uint64_t period)
        : Signal<uint8_t>(name, 1, 0), period_(period)
    {
        this->_kind = "clock";

        half_period_ = period_ / 2;
        // Needed to handle odd periods.
        remainder_ = period_ - half_period_;

        DSPSIM_CORO(tick);
    }

    Task<> Clock::tick()
    {
        while (true)
        {
            this->write(!this->read());
            co_await context()->wait(half_period_);
            this->write(!this->read());
            co_await context()->wait(remainder_);
        }
    }

    uint64_t Clock::period() const
    {
        return period_;
    }
} // namespace dspsim