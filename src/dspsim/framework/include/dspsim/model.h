#pragma once
#include <dspsim/context.h>
#include <string>

namespace dspsim
{
    using ModelPtr = std::shared_ptr<class Model>;
    class Model
    {
        friend class Context;

    private:
        Context *_context;
        size_t _id;
        std::string _name;
        std::string _hier_name;
        Model *_parent;

    public:
        Model(const std::string &name);

        // Methods.
        // Called during elaboration.
        virtual void finalize() {}

        // Simulation methods.
        virtual void eval() {}
        virtual void update() {}

        /*
            Properties
        */
        Context *context() const { return _context; }
        const std::string &name() const { return _name; }
        size_t id() const { return _id; }
        virtual const std::string kind() const { return "model"; }

        const std::string hier_name() const { return _hier_name; }
        Model *parent() const { return _parent; }
        //
        virtual const std::string repr() const;

        template <typename T, typename... Args>
        static auto create(Args &&...args)
        {
            auto m = std::make_shared<T>(std::forward<Args>(args)...);
            // Register the model with the context.
            m->context()->_own_model(m);
            return m;
        }
    };

}
