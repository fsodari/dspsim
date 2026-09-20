#pragma once
#include <dspsim/signal.h>

namespace dspsim
{
    class Clock : public Signal<uint8_t>
    {
    private:
        int _period;
        int _half_period;

    public:
        Clock(const std::string &name, int period);

        virtual void eval() override;
        using Signal<uint8_t>::update;

        int period() const;

        // static auto create(const std::string &name, int period)
        // {
        //     return Model::create<Clock>(name, period);
        // }
    };
    using ClockPtr = std::shared_ptr<Clock>;
} // namespace dspsim