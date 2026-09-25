#pragma once
#include <dspsim/signal.h>
#include <dspsim/process.h>

namespace dspsim
{
    class Clock final : public Signal<uint8_t>
    {
    private:
        int _period;
        int _half_period;
        Process *_process;

    public:
        Clock(const std::string &name, int period);
        int period() const;

        // Calls Signal<uint8_t>::update() and schedules the next time event.
        void update() override;

    private:
        void tick();

    public:
        static auto create(const std::string &name, int period)
        {
            return Model::create<Clock>(name, period);
        }
    };
    using ClockPtr = std::shared_ptr<Clock>;
} // namespace dspsim