#pragma once
// #include <dspsim/context.h>
#include <string>
#include <memory>

namespace dspsim
{
    class Context;
    using ModelPtr = std::shared_ptr<class Model>;
    class Model
    {
        friend class Context;

    private:
        Context *_context;
        uint32_t _id;
        std::string _name;
        std::string _hier_name;
        Model *_parent;

    protected:
        std::string _kind;

    public:
        Model(const std::string &name, const std::string &kind = "model");

        // Methods.
        // Called during elaboration.
        virtual void finalize();

        // Simulation methods.
        virtual void eval();
        // virtual void update();
        // Trace.
        virtual void dump_trace() {}

        /*
            Properties
        */
        Context *context() const;
        uint32_t id() const;
        const std::string &name() const;
        const std::string &kind() const;
        const std::string hier_name() const;
        Model *parent() const;
        //
        virtual const std::string repr() const;

        // template <typename T, typename... Args>
        // static auto create(Args &&...args)
        // {
        //     auto m = std::make_shared<T>(std::forward<Args>(args)...);
        //     // Register the model with the context.
        //     m->context()->_own_model(m);
        //     return m;
        // }
    };

}
