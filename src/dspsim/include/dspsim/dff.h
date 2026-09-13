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
        Dff(ClockPtr clk, int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false, const std::string &name = "");
        virtual void eval_step() override;
        virtual void eval_end_step() override;

        static auto create(ClockPtr clk, int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false, const std::string &name = "")
        {
            return Model::create<Dff<T>>(clk, width, init, is_signed, name);
        }

    protected:
        ClockPtr _clk;
        bool _update;
    };
} // namespace dspsim