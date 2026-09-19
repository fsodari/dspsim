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
        Module *m = _context->_active_module(); // Change to module stack.

        if (m)
        {
            _context->logger->info("Destructing ModuleName, {}, parent: {}", _name, m->name());
            m->_end_construction();
        }
        _context->_active_module_name_stack.pop_back();
    }

    const std::string &ModuleName::name() const
    {
        return _name;
    }

    ModuleName::operator const char *() const
    {
        return _name.c_str();
    }

    ModuleName::operator const std::string &() const
    {
        return _name;
    }
}