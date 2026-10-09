#include <dspsim/signal.h>
#include <dspsim/context.h>
#include <dspsim/port.h>
#include <dspsim/module.h>
#include <dspsim/event.h>

#include <spdlog/spdlog.h>

namespace dspsim
{
    SignalBase::SignalBase(const std::string &name)
        : Model(name, "signal"),
          scheduled_(false),
          change_event_(context()),
          posedge_event_(context()),
          negedge_event_(context())
    {
        context()->_add_signal(this);
    }

    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init)
        : SignalBase(name), width_(width)
    {
        d_ = init;
        q_ = init;
    }

    template <typename T>
    Signal<T> &Signal<T>::init(const T &value)
    {
        d_ = value;
        q_ = value;
        return *this;
    }

    template <typename T>
    void Signal<T>::write(const T &value)
    {
        d_ = value;

        if (d_ != q_) [[likely]]
        {
            // Schedule for update
            context()->_signal_update_stack.push_back(this);
        }
        // Erasing is probably more expensive than just ignoring a change during the update cycle.
        // else [[unlikely]]
        // {
        //     // If the signal is written more than once, and reset so that it no longer needs to be updated, remove it from the update stack.
        //     // This is an expensive operation. It would be ideal to avoid this, but some non-blocking assignment patterns
        //     // will write the same signal multiple times within the same update cycle.
        //     auto it = context()->_signal_update_stack.find(this);

        //     if (it != context()->_signal_update_stack.end())
        //     {
        //         context()->_signal_update_stack.erase(it);
        //     }
        // }
    }

    template <typename T>
    void Signal<T>::update()
    {
        if (d_ == q_) [[unlikely]]
        {
            return;
        }

        if (d_ && !q_)
        {
            pos().notify();
        }
        else if (!d_ && q_)
        {
            neg().notify();
        }
        change().notify();

        this->q_ = this->d_;
    }

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int8_t>;
    template class Signal<int16_t>;
    template class Signal<int32_t>;
    template class Signal<int64_t>;
    template class Signal<float>;
    template class Signal<double>;
}
