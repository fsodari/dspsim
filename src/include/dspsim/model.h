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
        // Context::own_object() for headers that only have a forward declaration of Context.
        void own_object(Context *context, std::shared_ptr<void> object);
    }

    class Model
    {
        friend class Context;

    public:
        Model(const std::string &name, const std::string &kind = "model");
        virtual ~Model() = default;
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
            // The model registered with the context when it was constructed. Give the context shared
            // ownership of it as well.
            detail::own_object(m->context(), m);
            return m;
        }

    protected:
        /// A model of a given context, whatever state the context is in. The other constructor takes the
        /// context that is under construction, so a model can only be created before its design is
        /// elaborated. This one is for a model that needs no elaboration and that nothing has to find
        /// during it, and that can therefore be created while the simulation runs: the signal of a field
        /// of a struct signal. finalize() is not called if the design is already elaborated.
        Model(Context *context, const std::string &name, const std::string &kind);

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
