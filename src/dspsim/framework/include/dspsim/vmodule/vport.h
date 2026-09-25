#pragma once
#include <dspsim/port.h>

namespace dspsim
{
    // Ports used for VModels. Can bind to the verilator model ports.
    template <typename T>
    class VInput final : public Input<T>
    {
    private:
        T *_ext_port;

    public:
        VInput(const std::string &name, int width, T &ext_port) : Input<T>(name, width)
        {
            bind_ext_port(ext_port);
        }

        // write the input value to the top model port.
        void _sync() override { *_ext_port = this->read(); }
        void bind_ext_port(T &ext_port) { _ext_port = &ext_port; }
    };

    template <typename T>
    class VOutput final : public Output<T>
    {
    private:
        T *_ext_port;

    public:
        // Initialize the port with the verilated model's port.
        VOutput(const std::string &name, int width, T &ext_port) : Output<T>(name, width)
        {
            bind_ext_port(ext_port);
        }

        // Write the top model value to the output port.
        void _sync() override { this->write(*_ext_port); }
        void bind_ext_port(T &ext_port) { _ext_port = &ext_port; }
    };

}

// Declare an input on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VINPUT(name, width) \
    ::dspsim::VInput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }

// Declare an output on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VOUTPUT(name, width) \
    ::dspsim::VOutput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }
