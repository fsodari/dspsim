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
          //   changed_flag_(false),
          //   posedge_flag_(false),
          //   negedge_flag_(false),
          scheduled_(false),
          change_event_(context()),
          posedge_event_(context()),
          negedge_event_(context())
    {
        context()->_add_signal(this);
    }

    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init, bool is_signed)
        : SignalBase(name), width_(width), is_signed_(is_signed)
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

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int>;
    template class Signal<float>;
    template class Signal<double>;
}
