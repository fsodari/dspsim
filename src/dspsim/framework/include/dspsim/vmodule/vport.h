#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/vmodule/vmodule_base.h>

namespace dspsim
{
    // Ports used for VModels. Can bind to the verilator model ports.
    template <typename T>
    class VInput : public Input<T>
    {
    private:
        T *_ext_port;
        VModuleBase *_vmodel_base;

    public:
        VInput(const std::string &name, int width = default_bitwidth<T>::value);
        VInput(const std::string &name, int width, T &ext_port);

        // virtual void _notify(EventType event) override;
        void _sync() override;
        void bind_ext_port(T &ext_port);
    };

    template <typename T>
    class VOutput : public Output<T>
    {
    private:
        T *_ext_port;
        VModuleBase *_vmodel_base;

    public:
        VOutput(const std::string &name, int width = default_bitwidth<T>::value);
        VOutput(const std::string &name, int width, T &ext_port);

        // virtual void _notify(EventType event) override;
        void _sync() override;
        void bind_ext_port(T &ext_port);
    };

}

#define DSPSIM_VINPUT(name, width) \
    ::dspsim::VInput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }

#define DSPSIM_VOUTPUT(name, width) \
    ::dspsim::VOutput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }
