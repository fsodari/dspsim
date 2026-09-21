#include <dspsim/dff.h>
#include <dspsim/context.h>
#include <dspsim/process.h>
#include <spdlog/spdlog.h>
namespace dspsim
{
    // template <typename T>
    // Dff<T>::Dff(ClockPtr clk, int width, T init, bool is_signed, const std::string &name)
    //     : Signal<T>(name, width, init, is_signed), _clk(clk), _update(false)
    // {
    // }

    template <typename T>
    Dff<T>::Dff(ModuleName name)
        : Module(name)
    {
        always << clk.pos();

        // always.link_process(clk.pos(), context()->register_process([this]()
        //                                                            { this->some_process(); }));

        // always.link_process(clk.pos(), context()->register_process(&Dff<T>::some_process, this));
        // auto x = std::remove_reference<decltype(*this)>::type;

        // context()->register_process(&Dff<T>::some_process, this);
        DSPSIM_PROCESS(&Dff<T>::some_process);
        always << clk.pos();
    }

    template <typename T>
    void Dff<T>::eval()
    {
        if (clk.read())
        {
            context()->logger->info("Dff eval: clk={}, d={}", clk.read(), d.read());
            q.write(d.read());
        }
    }

    template <typename T>
    void Dff<T>::some_process()
    {
        context()->logger->info("Dff some_process called!");
        // eval();
    }

    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;
} // namespace dspsim