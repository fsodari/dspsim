#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace dspsim
{
    class Context;
    class Model;

    namespace detail
    {
        // Context::own_model() for headers that only have a forward declaration of Context.
        void own_model(Context *context, std::shared_ptr<Model> model);
    }

    class Model
    {
        friend class Context;

    public:
        Model(const std::string &name, const std::string &kind = "model");
        ~Model() = default;
        /*
            Properties
        */
        // Defined inline: model code calls it on every evaluation (context()->wait(...)), and model code is
        // compiled outside of the core library, where link-time optimization cannot inline it.
        Context *context() const { return context_; }
        uint32_t id() const;
        const std::string &name() const;
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
            detail::own_model(m->context(), m);
            return m;
        }

    protected:
        // Called by the context during elaboration.
        virtual void finalize();
        // Called by the context at the end of every delta cycle, once the model has called request_trace().
        virtual void dump_trace();

        // Ask the context to call dump_trace() on this model at the end of every delta cycle.
        void request_trace();

    private:
        Context *context_;
        uint32_t id_;
        std::string name_;
        std::string hier_name_;
        Model *parent_;

    protected:
        std::string kind_;
    };

}
