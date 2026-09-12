#pragma once
#include <dspsim/signal.h>
#include <memory>

namespace dspsim
{
    template <typename T>
    class Input : public Model
    {
    protected:
        SignalPtr<T> sig;
        T &top_sig;

    public:
        Input(SignalPtr<T> _sig, T &_top_sig, int parent_id = -1)
            : Model("input"), sig(_sig), top_sig(_top_sig)
        {
            top_sig = sig->d();
            set_parent_id(parent_id);
        }
        void eval_step() override {}
        void eval_end_step() override
        {
            top_sig = sig->d();
        }
    };
    template <typename T>
    using InputPtr = std::shared_ptr<Input<T>>;

    template <typename T>
    class Output : public Model
    {
    protected:
        SignalPtr<T> sig;
        T &top_sig;

    public:
        Output(SignalPtr<T> _sig, T &top_sig, int parent_id = -1) : Model("output"), sig(_sig), top_sig(top_sig)
        {
            set_parent_id(parent_id);
            // sig->bind_output(&top_sig);
        }

        void eval_step() override
        {
            sig->set_d(top_sig);
        }
        void eval_end_step() override
        {
        }
    };
    template <typename T>
    using OutputPtr = std::shared_ptr<Output<T>>;
}
