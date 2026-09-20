#include <dspsim/model.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include <format>
namespace dspsim
{
    Model::Model(const std::string &name, const std::string &kind)
        : _context(Context::obtain().get()),
          _name(name),
          _parent(nullptr),
          _kind(kind)
    {
        // Register this model with the context.
        _parent = _context->_active_module();
        _context->_add_model(this);
    }

    void Model::finalize()
    {
    }

    void Model::eval()
    {
    }

    // void Model::update()
    // {
    // }

    Context *Model::context() const
    {
        return _context;
    }
    uint32_t Model::id() const
    {
        return _id;
    }
    const std::string &Model::name() const
    {
        return _name;
    }
    const std::string &Model::kind() const
    {
        return _kind;
    }

    const std::string Model::hier_name() const
    {
        return _hier_name;
    }
    Model *Model::parent() const
    {
        return _parent;
    }

    const std::string Model::repr() const
    {
        return std::format("Model(id={}, kind={}, name={}, parent={}, context={})", id(), kind(), name(), parent() ? parent()->name() : "root", context()->id());
    }
}