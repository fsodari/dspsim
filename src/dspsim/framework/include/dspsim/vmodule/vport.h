#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/vmodule/vmodule_base.h>

#define DSPSIM_VINPUT(name) \
    ::dspsim::VInput<std::remove_reference_t<decltype(top->name)>> name { #name, top->name }

#define DSPSIM_VOUTPUT(name) \
    ::dspsim::VOutput<std::remove_reference_t<decltype(top->name)>> name { #name, top->name }

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
        VInput(const std::string &name);
        VInput(const std::string &name, T &ext_port);

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
        VOutput(const std::string &name);
        VOutput(const std::string &name, T &ext_port);

        // virtual void _notify(EventType event) override;
        void _sync() override;
        void bind_ext_port(T &ext_port);
    };

}