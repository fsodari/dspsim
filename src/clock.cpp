#include <dspsim/clock.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

namespace dspsim
{
    Clock::Clock(const std::string &name, uint64_t period)
        : Signal<uint8_t>(name, 1, 0), period_(period)
    {
        this->kind_ = "clock";

        half_period_ = period_ / 2;
        // Needed to handle odd periods.
        remainder_ = period_ - half_period_;

        // next_delta_time_ = half_period_;

        DSPSIM_CORO(tick_coro);
        // DSPSIM_METHOD(tick_method);
    }

    std::shared_ptr<Clock> Clock::create(const std::string &name, uint64_t period)
    {
        return Model::create<Clock>(name, period);
    }

    uint64_t Clock::period() const
    {
        return period_;
    }

    Task<> Clock::tick_coro()
    {
        while (true)
        {
            this->write(!this->read());
            co_await context()->wait(half_period_);
            this->write(!this->read());
            co_await context()->wait(remainder_);
        }
    }

    // void Clock::tick_method()
    // {
    //     this->write(!this->read());
    //     next_delta_time_ = (next_delta_time_ == half_period_) ? remainder_ : half_period_;
    //     context()->schedule_time_delta_event(next_delta_time_);
    // }
} // namespace dspsim