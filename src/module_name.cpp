#include <dspsim/module_name.h>
#include <dspsim/context.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>
namespace dspsim
{
    ModuleName::ModuleName(const std::string &name) : _context(Context::obtain().get()), _name(name)
    {
        _context->logger->debug("Constructing ModuleName: {}", name);
        _context->_active_module_name_stack.push_back(this);
    }

    ModuleName::~ModuleName()
    {
        // Get the current active module in the hierarchy.
        Module *m = _context->_active_module();

        // Call _end_construction on the parent module to finalize module construction.
        if (m)
        {
            _context->logger->debug("Destructing ModuleName, {}, parent: {}", _name, m->hier_name());
            m->_end_construction();
        }
        else
        {
            // When would this happen? If a ModuleName was created outside the scope of a module?
            _context->logger->error("Error destructing ModuleName, {}, no active parent module", _name);
        }
        _context->_active_module_name_stack.pop_back();
    }
}
