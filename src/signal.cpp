#include <dspsim/signal.h>
#include <dspsim/context.h>
#include <dspsim/port.h>
#include <dspsim/module.h>
#include <dspsim/event.h>
#include <format>

#include "internal.h"

namespace dspsim
{
    SignalBase::SignalBase(const std::string &name)
        : Model(name, "signal"),
          _posedge_flag(false),
          _negedge_flag(false),
          _changed_flag(false)
    {
        context()->_add_signal(this);
    }

    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init, bool is_signed)
        : SignalBase(name), _width(width), _is_signed(is_signed)
    {
        _d = init;
        _q = init;
    }

    template <typename T>
    Signal<T> &Signal<T>::init(const T &value)
    {
        _d = value;
        _q = value;
        return *this;
    }

    template <typename T>
    void Signal<T>::write(const T &value)
    {
        _d = value;

        if (_d != _q)
        {
            // Schedule for update
            context()->_signal_update_stack.push_back(this);
        }
        else
        {
            // If the signal is written more than once, and reset so that it no longer needs to be updated, remove it from the update stack.
            // This is an expensive operation. It would be ideal to avoid this, but some non-blocking assignment patterns
            // will write the same signal multiple times within the same update cycle.
            auto it = context()->_signal_update_stack.find(this);

            if (it != context()->_signal_update_stack.end())
            {
                context()->_signal_update_stack.erase(it);
            }
        }
    }

    template <typename T>
    void Signal<T>::update()
    {
        EventType event = EventType::Changed;
        _changed_flag = true;
        context()->_signal_event = true;

        if (_d && !_q)
        {
            event = EventType::Posedge;
            _posedge_flag = true;
        }
        else if (!_d && _q)
        {
            event = EventType::Negedge;
            _negedge_flag = true;
        }

        this->_q = this->_d;

        // Notify modules sensitized directly to this signal (no intermediate Port).
        if (event == EventType::Posedge)
        {
            pos()->notify();
        }
        else if (event == EventType::Negedge)
        {
            neg()->notify();
        }

        _change()->notify();
    }

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int>;
    template class Signal<float>;
    template class Signal<double>;
}
