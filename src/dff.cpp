#include <dspsim/dff.h>
namespace dspsim
{
    template <typename T>
    Dff<T>::Dff(ClockPtr clk, int width, T init, bool is_signed)
        : Signal<T>(width, init, is_signed), _clk(clk), _update(false)
    {
    }

    template <typename T>
    void Dff<T>::eval_step()
    {
        _update = _clk->posedge();
    }

    template <typename T>
    void Dff<T>::eval_end_step()
    {
        if (_update)
        {
            this->_sync();
        }
    }

    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;
} // namespace dspsim