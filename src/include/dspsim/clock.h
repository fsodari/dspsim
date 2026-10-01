#pragma once
#include <dspsim/signal.h>
#include <dspsim/process.h>

namespace dspsim
{
    class Clock final : public Signal<uint8_t>
    {
    private:
        uint64_t _period;
        uint64_t _half_period;
        uint64_t _remainder;
        ProcessBase *_process;

    public:
        Clock(const std::string &name, uint64_t period);
        uint64_t period() const;

    private:
        // Toggles the clock signal and schedules the next time event.
        // Task tick();
        void tick();

    public:
        static auto create(const std::string &name, uint64_t period)
        {
            return Model::create<Clock>(name, period);
        }
    };
} // namespace dspsim
