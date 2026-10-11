#include <dspsim/model.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include <format>
namespace dspsim
{
    void detail::own_model(Context *context, std::shared_ptr<Model> model)
    {
        context->own_model(model);
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

    void Model::finalize()
    {
    }

    void Model::request_trace()
    {
        context_->trace_model(this);
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
        return std::format("Model(id={}, kind={}, name={}, parent={}, context={})", id(), kind(), name(), parent() ? parent()->name() : "root", context()->id());
    }
}