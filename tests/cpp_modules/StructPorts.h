#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/struct.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace demo
{
    /// A complex sample: a struct port, as a class template over the direction of its ports.
    template <template <typename> class Port>
    struct IqPorts : dspsim::PortStruct
    {
        Port<int8_t> re;
        Port<int8_t> im;

        IqPorts(const std::string &name) : PortStruct(name), re(name + ".re"), im(name + ".im")
        {
            add("re", re);
            add("im", im);
        }
    };

    /// The input of a stage: a struct port with an array and a nested struct.
    struct BusIn : dspsim::PortStruct
    {
        dspsim::Input<int16_t> data;
        dspsim::Input<uint8_t> valid;
        dspsim::InputArray<uint8_t> taps;
        IqPorts<dspsim::Input> iq;

        BusIn(const std::string &name)
            : PortStruct(name), data(name + ".data", 12), valid(name + ".valid", 1), taps(name + ".taps", {4}, 6), iq(name + ".iq")
        {
            add("data", data);
            add("valid", valid);
            add("taps", taps);
            add("iq", iq);
        }
    };

    struct BusOut : dspsim::PortStruct
    {
        dspsim::Output<int16_t> data;
        dspsim::Output<uint8_t> valid;
        dspsim::OutputArray<uint8_t> taps;
        IqPorts<dspsim::Output> iq;

        BusOut(const std::string &name)
            : PortStruct(name), data(name + ".data", 12), valid(name + ".valid", 1), taps(name + ".taps", {4}, 6), iq(name + ".iq")
        {
            add("data", data);
            add("valid", valid);
            add("taps", taps);
            add("iq", iq);
        }
    };

    /// Negates the data and the sample and reverses the taps of its bus, and inverts a 100-bit number.
    struct StructStage : public dspsim::Module
    {
        BusIn bus_in{"bus_in"};
        BusOut bus_out{"bus_out"};
        /// A struct port whose class is an instance of a class template.
        IqPorts<dspsim::Output> sum{"sum"};
        /// More than 64 bits: 32-bit words, least significant first.
        dspsim::InputArray<uint32_t> wide_in{"wide_in", {4}};
        dspsim::OutputArray<uint32_t> wide_out{"wide_out", {4}};
        /// Arrays of structs: the samples and their conjugates.
        dspsim::PortStructArrayOf<IqPorts<dspsim::Input>> samples{"samples", {3}};
        dspsim::PortStructArrayOf<IqPorts<dspsim::Output>> conjugates{"conjugates", {3}};

        StructStage(dspsim::ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always("*");
        }

        void eval()
        {
            bus_out.data = -bus_in.data.read();
            bus_out.valid = bus_in.valid.read();
            for (std::size_t i = 0; i < 4; ++i)
            {
                bus_out.taps[{i}] = bus_in.taps[{3 - i}].read();
                // The most significant word of a 100-bit number has 4 bits.
                wide_out[{i}] = ~wide_in[{i}].read() & (i == 3 ? 0xfu : 0xffffffffu);
            }
            bus_out.iq.re = -bus_in.iq.re.read();
            bus_out.iq.im = -bus_in.iq.im.read();
            sum.re = bus_in.iq.re.read() + bus_in.iq.im.read();
            sum.im = bus_in.iq.re.read() - bus_in.iq.im.read();
            for (std::size_t i = 0; i < 3; ++i)
            {
                conjugates[{i}].re = samples[{i}].re.read();
                conjugates[{i}].im = -samples[{i}].im.read();
            }
        }

    private:
        // Not a port of the module.
        dspsim::PortStruct unused_{"unused"};
    };
} // namespace demo
