#include <dspsim/context.h>
#include <dspsim/model.h>

#include <string>
#include <format>
#include <ranges>

namespace dspsim
{
    // Global context pointer. Models will self-register with the global context.
    static ContextPtr _global_context = std::make_shared<Context>();
    static int next_context_id = 0;

    ContextPtr Context::global_context()
    {
        return _global_context;
    }

    Context::Context() : _id(next_context_id++), _next_model_id(0)
    {
    }

    Context::~Context()
    {
        clear();
    }
    void Context::clear()
    {
        _models.clear();
    }

    const std::string Context::repr() const
    {
        std::string models_str;
        for (const auto &model : _models)
        {
            if (!models_str.empty())
            {
                models_str += ", ";
            }
            models_str += model->repr();
        }

        return std::format("Context(id={}, models=[{}])", _id, models_str);
    }

    ContextPtr Context::reset()
    {
        auto context = std::make_shared<Context>();

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
        model->_id = _next_model_id++;
        _models.push_back(model);
    }

    void Context::elaborate()
    {
        // // Assign ids to every model.
        // for (size_t i = 0; i < _models.size(); ++i)
        // {
        //     _models[i]->_id = _next_model_id++;
        // }

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