#include <dspsim/process.h>

namespace dspsim
{
    uint32_t Process::id() const
    {
        return _id;
    }

    Process::Process(Context *context, uint32_t id, std::function<void()> eval)
        : _context(context), _id(id), eval(eval)
    {
    }
}