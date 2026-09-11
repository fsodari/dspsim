#include <dspsim/model.h>
#include <string>
#include <format>
#include <ranges>

namespace dspsim
{
    Model::Model() : _context(Context::obtain())
    {
    }

    const std::string Model::repr() const
    {
        return std::format("Model(id={})", _id);
    }
}