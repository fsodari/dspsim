#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    class ModuleName
    {
    private:
        ContextPtr _context;
        std::string _name;

    public:
        ModuleName(const std::string &name);
        ModuleName(const char *name);
        ~ModuleName();
        const std::string &name() const;
        operator const char *() const;
        operator const std::string &() const;
    };
}