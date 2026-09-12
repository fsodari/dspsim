#include <dspsim/signal.h>
#include <format>

namespace dspsim
{
    template <typename T>
    Signal<T>::Signal(int width, T init, bool is_signed)
        : Model("signal"),
          _width(width),
          _is_signed(is_signed),
          _parent_id(-1)
    {
        _d_local = init;
        _q_local = init;
        _d = &_d_local;
        _q = &_q_local;
        _q_prev = init;
    }

    template <typename T>
    Signal<T>::~Signal()
    {
    }

    template <typename T>
    const std::string Signal<T>::repr() const
    {
        return std::format("Signal(id={}, kind={}, width={}, is_signed={}, parent_id={})", id(), kind(), width(), is_signed(), parent_id());
    }

    template <typename T>
    void Signal<T>::eval_end_step()
    {
        _q_prev = *_q;
        *_q = *_d;
    }

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
}
