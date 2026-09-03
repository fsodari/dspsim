#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    using ModelPtr = std::shared_ptr<class Model>;
    class Model
    {
        friend class Context;

    public:
        Model();

        virtual void eval_step() {}
        virtual void eval_end_step() {}

        int id() const { return _id; }
        ContextPtr context() const { return _context; }

        template <typename T, typename... Args>
        static ModelPtr create(Args &&...args)
        {
            auto m = std::make_shared<T>(std::forward<Args>(args)...);
            // Register the model with the context.
            m->_context->register_model(m);
            return m;
        }

    private:
        ContextPtr _context;
        int _id;
    };
}