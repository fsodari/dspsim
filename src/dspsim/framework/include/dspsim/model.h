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
        ContextPtr _context;
        int _id;
        std::string _name;
        std::string _hier_name;
        Model *_parent;

    public:
        Model(const std::string &name);

        // Called during elaboration.
        virtual void finalize() {}

        // Simulation methods.
        virtual void eval() {}
        virtual void update() {}

        /*
            Properties
        */
        ContextPtr context() const { return _context; }
        int id() const { return _id; }
        virtual const std::string kind() const { return "model"; }

        const std::string &name() const { return _name; }
        void set_name(const std::string &name) { _name = name; }

        const std::string hier_name() const { return _hier_name; }
        Model *parent() const { return _parent; }
        // int parent_id() const { return _parent_id; }
        // void set_parent_id(int parent_id) { _parent_id = parent_id; }

        // /*
        //     Methods
        // */
        // virtual void eval_step() = 0;
        // virtual void eval_end_step() {}

        //
        virtual const std::string repr() const;

        template <typename T, typename... Args>
        static auto create(Args &&...args)
        {
            auto m = std::make_shared<T>(std::forward<Args>(args)...);
            // Register the model with the context.
            m->context()->own_model(m);
            return m;
        }
    };

}
