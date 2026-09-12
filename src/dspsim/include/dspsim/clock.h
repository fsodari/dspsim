#pragma once
#include <dspsim/signal.h>

namespace dspsim
{
    class Clock : public Signal<uint8_t>
    {
    public:
        Clock(int period);
        virtual void eval_step() override;
        virtual void eval_end_step() override;

        int period() const { return _period; }

        bool posedge() const { return q() && !_q_prev; }

        static auto create(int period)
        {
            return Model::create<Clock>(period);
        }

    private:
        int _period;
        int _half_period;
        uint8_t _q_prev;
        uint64_t _checkpoint;
    };
    using ClockPtr = std::shared_ptr<Clock>;
} // namespace dspsim