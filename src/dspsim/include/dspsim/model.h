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

        // Create a model as a shared ptr and register it with the context.
        template <typename M, typename... Args>
        static std::shared_ptr<M> create(Args &&...args)
        {
            auto m = std::make_shared<M>(std::forward<Args>(args)...);

            // Add to the owned models.
            m->context()->m_owned_models.push_back(m);
            return m;
        }

    protected:
        Context *m_context;
    };

    using ModelPtr = std::shared_ptr<Model>;

} // namespace dspsim
