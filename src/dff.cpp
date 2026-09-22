#include <dspsim/dff.h>
#include <dspsim/context.h>
#include <dspsim/process.h>
#include <spdlog/spdlog.h>
namespace dspsim
{
    template <typename T>
    Dff<T>::Dff(ModuleName name)
        : Module(name)
    {
        DSPSIM_METHOD(&Dff<T>::some_process);
        sensitive << clk.pos();
    }

    template <typename T>
    void Dff<T>::some_process()
    {
        if (clk.read())
        {
            context()->logger->info("Dff some_process called!");
            q.write(d.read());
        }
    }

    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;
} // namespace dspsim