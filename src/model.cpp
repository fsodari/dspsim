#include <dspsim/model.h>
#include <dspsim/context.h>
#include <dspsim/module.h>

#include <string>
namespace dspsim
{
    void detail::own_object(Context *context, std::shared_ptr<void> object)
    {
        context->own_object(std::move(object));
    }

    Model::Model(const std::string &name, const std::string &kind)
        : context_(Context::obtain().get()),
          id_(context_->next_model_id()),
          name_(name),
          parent_(nullptr),
          kind_(kind)
    {
        // Set a default name if none is provided.
        if (name_.empty())
        {
            name_ = kind_ + std::to_string(id_);
        }

        // Register this model with the context.
        parent_ = context_->active_module();
        context_->add_model(this);
    }

    uint32_t Model::id() const
    {
        return id_;
    }

    const std::string &Model::name() const
    {
        return name_;
    }

    const std::string &Model::kind() const
    {
        return kind_;
    }

    const std::string Model::hier_name() const
    {
        return hier_name_;
    }
    Model *Model::parent() const
    {
        return parent_;
    }

    const std::string Model::repr() const
    {
        return "Model(id=" + std::to_string(id()) + ", kind=" + kind() + ", name=" + name() +
               ", parent=" + (parent() ? parent()->name() : std::string("root")) +
               ", context=" + std::to_string(context()->id()) + ")";
    }

    void Model::finalize()
    {
    }

    void Model::dump_trace()
    {
    }

    void Model::request_trace()
    {
        context_->trace_model(this);
    }
}