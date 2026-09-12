#pragma once
#include <dspsim/context.h>
#include <string>

namespace dspsim
{
    using ModelPtr = std::shared_ptr<class Model>;
    class Model
    {
        friend class Context;

    public:
        Model(const std::string &kind = "model");

        /*
            Properties
        */
        ContextPtr context() const { return _context; }
        int id() const { return _id; }
        const std::string &kind() const { return _kind; }

        int parent_id() const { return _parent_id; }
        void set_parent_id(int parent_id) { _parent_id = parent_id; }

        /*
            Methods
        */
        virtual void eval_step() = 0;
        virtual void eval_end_step() {}

        //
        virtual const std::string repr() const;

        template <typename T, typename... Args>
        static auto create(Args &&...args)
        {
            auto m = std::make_shared<T>(std::forward<Args>(args)...);
            // Register the model with the context.
            m->context()->register_model(m);
            return m;
        }

    private:
        ContextPtr _context;
        int _id;

    protected:
        std::string _kind;
        int _parent_id;
    };

}
