#pragma once
#include <dspsim/signal.h>
#include <dspsim/process.h>

namespace dspsim
{
    class Clock final : public Signal<uint8_t>
    {
    public:
        Clock(const std::string &name, uint64_t period);
        static auto create(const std::string &name, uint64_t period)
        {
            return Model::create<Clock>(name, period);
        }

        uint64_t period() const;

    private:
        // Toggles the clock signal and schedules the next time event.
        Task<> tick_coro();
        // void tick_method();

    private:
        uint64_t period_;
        uint64_t half_period_;
        uint64_t remainder_;
        // uint64_t next_delta_time_;
    };
} // namespace dspsim
