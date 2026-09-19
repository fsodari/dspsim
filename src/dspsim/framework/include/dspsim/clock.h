#pragma once
#include <dspsim/signal.h>

namespace dspsim
{
    class Clock : public Signal<uint8_t>
    {
    public:
        Clock(const std::string &name, int period);
        virtual const std::string kind() const override { return "clock"; }

        virtual void eval() override;
        using Signal<uint8_t>::update;

        int period() const { return _period; }

        // int period() const { return _period; }

        // bool posedge() const { return q() && !_q_prev; }

        static auto create(const std::string &name, int period)
        {
            return Model::create<Clock>(name, period);
        }

    private:
        int _period;
        int _half_period;
    };
    using ClockPtr = std::shared_ptr<Clock>;
} // namespace dspsim