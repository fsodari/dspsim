#include <dspsim/model.h>
#include <format>
namespace dspsim
{
    Model::Model(const std::string &kind)
        : _context(Context::obtain()),
          _id(_context->get_next_model_id()),
          _kind(kind),
          _parent_id(-1)
    {
    }

    const std::string Model::repr() const
    {
        return std::format("Model(id={}, kind={}, context={})", id(), kind(), context()->id());
    }

}