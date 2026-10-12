#include <dspsim/channel.h>
#include <dspsim/context.h>

namespace dspsim
{
    detail::Updatable::~Updatable() = default;

    Channel::Channel(Context *context) : context_(context)
    {
    }

    void Channel::request_update()
    {
        context_->post_update_stack_.push_back(this);
    }

    void Channel::commit()
    {
        update();
    }
} // namespace dspsim
