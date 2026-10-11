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
              module_depth(context->active_module_stack_.size())
        {
            context->logger_->debug("Constructing ModuleName: {}", name);
            context->active_module_name_stack_.push_back(name);
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
            if (context->active_module_stack_.size() > module_depth)
            {
                Module *m = context->active_module();
                context->logger_->debug("Ending construction of {}", m->hier_name());
                m->end_construction();
            }
            else
            {
                context->logger_->error("ModuleName {} ended without constructing a module", name);
            }
            // Context::clear() may have emptied the stack while this scope was open.
            if (!context->active_module_name_stack_.empty())
            {
                context->active_module_name_stack_.pop_back();
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
