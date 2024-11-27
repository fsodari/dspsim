#include "dspsim/model.h"

namespace dspsim
{
    ModelBase::ModelBase() : context(Context::context())
    {
        // Register with the unowned models automatically.
        context->register_model(this);
    }
} // namespace dspsim
