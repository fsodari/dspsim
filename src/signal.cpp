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
        : Model(name, "signal")
    {
        context()->_add_signal(this);
    }

    SensitivityEvent &SignalBase::pos()
    {
        return _posedge_event;
    }
    SensitivityEvent &SignalBase::neg()
    {
        return _negedge_event;
    }
    SensitivityEvent &SignalBase::_change()
    {
        return _change_event;
    }
    SignalBase::operator SensitivityEvent &()
    {
        return _change();
    }

    bool SignalBase::posedge() const
    {
        return _posedge_flag;
    }
    bool SignalBase::negedge() const
    {
        return _negedge_flag;
    }
    bool SignalBase::changed() const
    {
        return _changed_flag;
    }
    void SignalBase::_clear_event_flag()
    {
        _posedge_flag = false;
        _negedge_flag = false;
        _changed_flag = false;
    }

    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init, bool is_signed)
        : SignalBase(name),
          _width(width),
          _is_signed(is_signed)
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
    int Signal<T>::width() const
    {
        return _width;
    }

    template <typename T>
    bool Signal<T>::is_signed() const
    {
        return _is_signed;
    }

    template <typename T>
    void Signal<T>::write(const T &value)
    {
        _d = value;

        if (_d != _q)
        {
            SPDLOG_LOGGER_TRACE(context()->logger, "Signal {} scheduled for update", name());
            // Schedule for update
            context()->_signal_update_stack.push_back(this);
        }
        else
        {
            // If the signal is written more than once, and reset so that it no longer needs to be updated, remove it from the update stack.
            auto it = context()->_signal_update_stack.find(this);

            if (it != context()->_signal_update_stack.end())
            {
                context()->_signal_update_stack.erase(it);
                SPDLOG_LOGGER_TRACE(context()->logger, "Signal {} removed from update stack", name());
            }
        }
    }

    template <typename T>
    const T &Signal<T>::read() const
    {
        return _q;
    }
    template <typename T>
    const T &Signal<T>::_read_d() const
    {
        return _d;
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
            for (auto module : pos().subscribers())
            {
                context()->_push_eval_stack(module);
            }
        }
        else if (event == EventType::Negedge)
        {
            for (auto module : neg().subscribers())
            {
                context()->_push_eval_stack(module);
            }
        }
        for (auto module : _change().subscribers())
        {
            SPDLOG_LOGGER_TRACE(context()->logger, "Signal {} notifying changed subscriber: {}", name(), module->name());
            context()->_push_eval_stack(module);
        }
    }

    template <typename T>
    const std::string Signal<T>::repr() const
    {
        // return std::format("Signal(id={}, kind={}, width={}, is_signed={}, name={}, parent_id={})", id(), kind(), width(), is_signed(), name(), parent_id());
        return "";
    }

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int>;
}
