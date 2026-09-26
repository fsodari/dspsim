#pragma once
#include <string>
#include <memory>

namespace dspsim
{
    class Context;
    class Model;

    void _own_model_helper(Context *context, std::shared_ptr<Model> model);

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

        // Trace.
        virtual void dump_trace() {}

        /*
            Properties
        */
        // Defined inline: called on every signal/process bookkeeping operation in the
        // delta-cycle hot path, so it must be inlinable without relying on LTO.
        Context *context() const { return _context; }
        uint32_t id() const { return _id; }
        const std::string &name() const { return _name; }
        const std::string &kind() const;
        const std::string hier_name() const;
        Model *parent() const;
        //
        virtual const std::string repr() const;

        template <typename T, typename... Args>
        static auto create(Args &&...args)
        {
            auto m = std::make_shared<T>(std::forward<Args>(args)...);
            // Register the model with the context and give shared ownership to the context.
            _own_model_helper(m->context(), m);
            return m;
        }
    };

}
