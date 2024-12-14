#include "dspsim/model.h"

namespace dspsim
{
    Model::Model() : m_context(Context::obtain().get())
    {
        m_context->register_model(this);
    }
} // namespace dspsim
