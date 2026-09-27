#pragma once
#include <string>

namespace dspsim
{
    class Context;
    /*
        Helper class to facilitate calling Module::_end_construction() after the Module's constructor.
        ModuleName should not be instantiated directly. It should be an argument for a Module subclass.
        When the module constructor is called, ModuleName will get allocated and call its constructor, then
        when the Module constructor finishes, the ModuleName will go out of scope and its destructor will be called, triggering the call to Module::_end_construction().

        When a ModuleName is created, it will push its name onto the active module name stack in the context.
        If the Module base class is called without args, it retrieves the name from the stack.

        It generally behaves like a string otherwise.

        Python constructors will need to handle this by creating a ModuleName before constructing a class, then deleting
        the ModuleName after __init__ is called. Python has facilities to wrap this behavior around class constructors.

        Problems: Nesting derived classes gets tricky. The final class must be the only one to use ModuleName directly in its constructor.
        So it must have some way of propogating a reference through the base classes. Using a default constructor could work as the modulename
        will be on the stack.
    */
    class ModuleName
    {
    private:
        Context *_context;
        std::string _name;

    public:
        ModuleName(const std::string &name);
        ModuleName(const char *name) : ModuleName(std::string(name)) {}

        // This will call Module::_end_construction() on the active module in the context hierarchy.
        ~ModuleName();

        const std::string &name() const { return _name; }
        operator const char *() const { return _name.c_str(); }
        operator const std::string &() const { return _name; }
    };
}