#include <dspsim/process.h>

namespace dspsim
{

    Process::Process(Context *context, uint32_t id, std::function<void()> eval, Model *source, const std::string &name)
        : _context(context), _source(source), _id(id), eval(eval), _name(name)
    {
    }
}