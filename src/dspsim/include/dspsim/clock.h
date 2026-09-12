#pragma once
#include <dspsim/signal.h>

namespace dspsim
{
    class Clock : public Signal<uint8_t>
    {
    public:
        Clock(int period);
        virtual void eval_step() override;

        int period() const { return _period; }

        static auto create(int period)
        {
            return Model::create<Clock>(period);
        }

    private:
        int _period;
        int _half_period;
        uint64_t _checkpoint;
    };
} // namespace dspsim