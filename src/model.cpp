#include "dspsim/model.h"

namespace dspsim
{
    Model::Model() : m_context(Context::obtain())
    {
        m_context->register_model(this);
    }
} // namespace dspsim
