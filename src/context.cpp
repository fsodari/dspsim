#include <dspsim/context.h>
#include <dspsim/model.h>

namespace dspsim
{
    // Global context pointer. Models will self-register with the global context.
    static ContextPtr _global_context = nullptr;

    // Initialize each new context with a new id.
    static int next_context_id = 0;

    Context::Context() : _id(next_context_id++), _next_model_id(0), _time(0)
    {
    }

    Context::~Context()
    {
        clear();
    }

    void Context::register_model(ModelPtr model)
    {
        model->_id = _next_model_id++;
        _models.push_back(model);
    }

    void Context::clear()
    {
        _models.clear();
    }

    ContextPtr Context::obtain()
    {
        if (_global_context == nullptr)
        {
            _global_context = std::shared_ptr<Context>(new Context());
        }
        return _global_context;
    }

    void Context::reset_global_context()
    {
        _global_context = nullptr;
    }
}