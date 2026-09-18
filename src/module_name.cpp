#include <dspsim/module_name.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>
namespace dspsim
{
    ModuleName::ModuleName(const std::string &name) : _context(Context::obtain()), _name(name)
    {
        _context->logger->info("Constructing ModuleName: {}", name);
        _context->_active_module_name_stack.push_back(this);
    }

    ModuleName::ModuleName(const char *name) : ModuleName(std::string(name))
    {
    }

    ModuleName::~ModuleName()
    {
        Module *m = _context->active_module(); // Change to module stack.
        if (m)
        {
            m->end_construction();
        }
        _context->_active_module_name_stack.pop_back();
        _context->logger->info("Destructing ModuleName");
    }
}