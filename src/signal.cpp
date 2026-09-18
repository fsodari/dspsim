#include <dspsim/signal.h>
#include <dspsim/port.h>
#include <format>

#include "internal.h"

namespace dspsim
{
    SignalBase::SignalBase(const std::string &name)
        : Model(name)
    {
        // context()->register_signal(this);
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
    Signal<T>::~Signal()
    {
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
            port->notify(event);
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
