#pragma once
#include <dspsim/model.h>
#include <dspsim/signal.h>
#include <dspsim/clock.h>

namespace dspsim
{
    template <typename T>
    class Dff : public Signal<T>
    {
    public:
        Dff(ClockPtr clk, int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false);
        virtual void eval_step() override;
        virtual void eval_end_step() override;

        static auto create(ClockPtr clk, int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false)
        {
            return Model::create<Dff<T>>(clk, width, init, is_signed);
        }

    protected:
        ClockPtr _clk;
        bool _update;
    };
} // namespace dspsim