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
          _change_event(context()),
          _posedge_event(context()),
          _negedge_event(context()),
          _changed_flag(false),
          _posedge_flag(false),
          _negedge_flag(false),
          _scheduled(false)
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

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int>;
    template class Signal<float>;
    template class Signal<double>;
}
