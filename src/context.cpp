#include <dspsim/context.h>
#include <dspsim/model.h>

namespace dspsim
{
    Context::Context() : _id(0)
    {
    }

    ContextPtr Context::reset()
    {
        static int next_id = 0;
        auto context = std::make_shared<Context>();
        context->_id = next_id++;

        // Assign the new context to the global context pointer.
        _global_context = context;

        return context;
    }

    ContextPtr Context::obtain()
    {
        return _global_context;
    }

    void Context::register_model(std::shared_ptr<Model> model)
    {
        _models.push_back(model);
    }

    void Context::elaborate()
    {
        // Assign ids to every model.
        for (size_t i = 0; i < _models.size(); ++i)
        {
            _models[i]->_id = i;
        }

        // Detach the context from the global context.
        Context::reset();
    }

    void Context::eval()
    {
        for (auto &model : _models)
        {
            model->eval_step();
        }
        for (auto &model : _models)
        {
            model->eval_end_step();
        }
    }
}