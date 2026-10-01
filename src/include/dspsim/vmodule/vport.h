#pragma once
#include <dspsim/port.h>

namespace dspsim
{
    // Ports used for VModels. Can bind to the verilator model ports.
    template <typename T>
    class VInput final : public Input<T>
    {

    public:
        VInput(const std::string &name, int width, T &ext_port) : Input<T>(name, width)
        {
            ext_port_ = &ext_port;
        }

        // write the input value to the top model port.
        void sync() override { *ext_port_ = this->read(); }

    private:
        T *ext_port_;
    };

    template <typename T>
    class VOutput final : public Output<T>
    {

    public:
        // Initialize the port with the verilated model's port.
        VOutput(const std::string &name, int width, T &ext_port) : Output<T>(name, width)
        {
            ext_port_ = &ext_port;
        }

        // Write the top model value to the output port.
        void sync() override { this->write(*ext_port_); }

    private:
        T *ext_port_;
    };

}

// Declare an input on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VINPUT(name, width) \
    ::dspsim::VInput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }

// Declare an output on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VOUTPUT(name, width) \
    ::dspsim::VOutput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }
