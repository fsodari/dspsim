#include <dspsim/dff.h>
namespace dspsim
{
    // template <typename T>
    // Dff<T>::Dff(ClockPtr clk, int width, T init, bool is_signed, const std::string &name)
    //     : Signal<T>(name, width, init, is_signed), _clk(clk), _update(false)
    // {
    // }

    template <typename T>
    Dff<T>::Dff(ModuleName name, Signal<uint8_t> &clk_, Signal<T> &d_, Signal<T> &q_)
        : Module(name),
          clk("clk", clk_),
          d("d", d_),
          q("q", q_)
    {
        always << clk.pos();
    }

    template <typename T>
    void Dff<T>::eval()
    {
        if (clk.read())
        {
            q.write(d.read());
        }
    }

    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;
} // namespace dspsim