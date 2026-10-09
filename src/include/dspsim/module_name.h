#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace dspsim
{
    class Context;

    /*
        Scopes the construction of a Module so that submodules, ports, and signals declared as members get
        hierarchical names.

        Creating a ModuleName opens a construction scope: its name is pushed onto the context's active module name
        stack, and the Module constructor pushes the module onto the active module stack. Ending the scope pops both,
        which finishes the module's construction.

        A Module subclass takes ModuleName by value. The argument lives until the subclass constructor returns, so the
        scope covers the whole member-initializer list. Copies share one scope, which ends when the last copy is
        destroyed. A class derived from another module can therefore pass its ModuleName on by value, and the scope
        lasts until the most-derived constructor returns.

        Python calls end() explicitly (through the context-manager protocol) because it has no RAII.
    */
    class ModuleName
    {
    public:
        ModuleName(const std::string &name);
        ModuleName(const char *name) : ModuleName(std::string(name)) {}

        /// Copies share the construction scope.
        ModuleName(const ModuleName &) = default;
        ModuleName &operator=(const ModuleName &) = delete;

        /// End the construction scope now, instead of when the last copy is destroyed. Calling it again does nothing.
        void end();

        const std::string &name() const;
        operator const char *() const { return name().c_str(); }
        operator const std::string &() const { return name(); }

    private:
        struct Scope;
        std::shared_ptr<Scope> scope_;
    };
}
