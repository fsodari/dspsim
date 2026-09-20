#pragma once
#include <string>
namespace dspsim
{
    class Context;
    class ModuleName
    {
    private:
        Context *_context;
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