#include <dspsim/module_name.h>
#include <dspsim/context.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>
namespace dspsim
{
    /*
        Shared by all copies of a ModuleName. Ends construction when the last copy goes away.
    */
    struct ModuleName::Scope
    {
        Context *context;
        std::string name;
        // Size of the active module stack when the scope opened. The Module constructed in this scope sits above it.
        std::size_t module_depth;
        bool ended = false;

        Scope(Context *context, const std::string &name)
            : context(context),
              name(name),
              module_depth(context->_active_module_stack.size())
        {
            context->logger->debug("Constructing ModuleName: {}", name);
            context->_active_module_name_stack.push_back(name);
        }

        ~Scope()
        {
            end();
        }

        void end()
        {
            if (ended)
            {
                return;
            }
            ended = true;

            // Only pop a module that was constructed in this scope. If the Module constructor never ran
            // (for example it threw), the active module belongs to a parent and must stay.
            if (context->_active_module_stack.size() > module_depth)
            {
                Module *m = context->_active_module();
                context->logger->debug("Ending construction of {}", m->hier_name());
                m->_end_construction();
            }
            else
            {
                context->logger->error("ModuleName {} ended without constructing a module", name);
            }
            // Context::clear() may have emptied the stack while this scope was open.
            if (!context->_active_module_name_stack.empty())
            {
                context->_active_module_name_stack.pop_back();
            }
        }
    };

    ModuleName::ModuleName(const std::string &name)
        : scope_(std::make_shared<Scope>(Context::obtain().get(), name))
    {
    }

    void ModuleName::end()
    {
        scope_->end();
    }

    const std::string &ModuleName::name() const
    {
        return scope_->name;
    }
}
