#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    class ModelBase
    {
    public:
        // Initialize model. set the context to the default global context.
        ModelBase();
        // Required interface.
        virtual void eval_step() = 0;
        virtual void eval_end_step() = 0;

    protected:
        const std::shared_ptr<Context> context;
    };

    // Crtp mixin class.
    template <class M>
    class Model : public ModelBase
    {
    public:
        void eval_step()
        {
            static_cast<M *>(this)->eval_step();
        }

        void eval_end_step()
        {
            static_cast<M *>(this)->eval_end_step();
        }

        // Create a shared ptr of this class. Used for implementing __new__ in Python.
        template <class... Args>
        static std::shared_ptr<M> create(Args &&...args)
        {
            auto new_model = std::make_shared<M>(args...);
            new_model->context->register_model(new_model);
            return new_model;
        }
    };
} // namespace dspsim
