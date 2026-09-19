#include <dspsim/model.h>
#include <dspsim/module.h>
#include <format>
namespace dspsim
{
    Model::Model(const std::string &name)
        : _context(Context::obtain().get()),
          _name(name),
          _parent(nullptr)
    {
        // Register this model with the context.
        _parent = _context->_active_module();
        _context->_add_model(this);
    }

    const std::string Model::repr() const
    {
        return std::format("Model(id={}, kind={}, name={}, parent={}, context={})", id(), kind(), name(), parent() ? parent()->name() : "root", context()->id());
    }
}