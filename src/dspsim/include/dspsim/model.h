#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    class Model
    {
    public:
        Model();
        virtual void eval_step() = 0;
        virtual void eval_end_step() {}

        Context *context() const { return m_context; }

    protected:
        Context *m_context;
    };

    using ModelPtr = std::shared_ptr<Model>;

    template <class M, class... Args>
    std::shared_ptr<M> create(Args &&...args)
    {
        std::shared_ptr<M> new_model = std::make_shared<M>(args...);
        new_model->context()->own_model(new_model);
        return new_model;
    }
} // namespace dspsim
