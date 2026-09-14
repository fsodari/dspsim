#include <dspsim/model.h>
#include <format>
namespace dspsim
{
    Model::Model(const std::string &kind, const std::string &name)
        : _context(Context::obtain()),
          _id(_context->get_next_model_id()),
          _kind(kind),
          _name(name),
          _parent_id(-1)
    {
    }

    const std::string Model::repr() const
    {
        return std::format("Model(id={}, kind={}, name={}, parent={}, context={})", id(), kind(), name(), parent_id(), context()->id());
    }

}