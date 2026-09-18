#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    class ModuleName
    {
    public:
        ModuleName(const std::string &name);
        ModuleName(const char *name);
        ~ModuleName();
        const std::string &name() const { return _name; }
        operator const char *() const { return _name.c_str(); }
        operator const std::string &() const { return _name; }

    private:
        ContextPtr _context;
        std::string _name;
    };
}