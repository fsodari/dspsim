#include <dspsim/signal.h>
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
    }

    void SignalBase::_add_driver(PortBase *driver)
    {
        _drivers.push_back(driver);
    }
    void SignalBase::_add_subscriber(PortBase *subscriber)
    {
        _subscribers.push_back(subscriber);
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
        SPDLOG_LOGGER_TRACE(context()->logger, "Signal {} scheduled for eval", name());
        // Schedule for eval.
        context()->_push_eval_stack(this);
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
    void Signal<T>::eval()
    {
        if (_d != _q)
        {
            SPDLOG_LOGGER_TRACE(context()->logger, "Signal eval() value changed: {}", name());
        }
    }

    template <typename T>
    void Signal<T>::update()
    {
        // If no change, don't update subscribers.
        if (_d == _q)
        {
            return;
        }
        EventType event = EventType::Changed;
        if (_d && !_q)
        {
            event = EventType::Posedge;
        }
        else if (!_d && _q)
        {
            event = EventType::Negedge;
        }

        this->_q = this->_d;

        for (auto port : _subscribers)
        {
            SPDLOG_LOGGER_TRACE(context()->logger, "Signal {} notifying subscriber: {}, event: {}", name(), port->name(), static_cast<int>(event));
            port->_notify(event);
        }

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
