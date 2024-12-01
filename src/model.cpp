#include "dspsim/model.h"

namespace dspsim
{
    Model::Model() : m_context(Context::context().get())
    {
        this->context()->register_model(this);
    }
    // ModelBase::ModelBase() : context(Context::context().get())
    // {
    //     // if (auto ctx = context.lock())
    //     // {
    //     //     // Register with the unowned models automatically.
    //     //     ctx->register_model(this);
    //     // }
    //     context->register_model(this);
    // }
} // namespace dspsim
