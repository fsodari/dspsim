/*
    SystemC counterparts of verilated.h: the SystemC models that Verilator writes for hdl/Bus.sv (--sc), in the
    same pipeline. Verilator gives a SystemC model a port of 32 bits for every port up to that, and one port
    of 64 bits for the packed struct. It does not write a SystemC model for a module with an unpacked struct
    port.
*/
#pragma once
#include "verilated_values.h"

#include <systemc>

#include <VBusPackedSC.h>
#include <VBusPortsSC.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /*
        A kind has the type Stage, the type Link of the signals of a link, constructed from a name,
        bind_s(stage, link) and bind_m(stage, link), read(link) and write(link, value).
    */
    /// Kind "ports": a port and a signal per field.
    struct VBusFieldsSC
    {
        using Stage = ::VBusPortsSC;

        struct Link
        {
            sc_core::sc_signal<uint32_t> data;
            sc_core::sc_signal<uint32_t> tag;
            sc_core::sc_signal<uint32_t> re;
            sc_core::sc_signal<uint32_t> im;

            explicit Link(const std::string &name)
                : data((name + "_data").c_str()), tag((name + "_tag").c_str()), re((name + "_iq_re").c_str()), im((name + "_iq_im").c_str())
            {
            }
        };

        static void bind_s(Stage &stage, Link &link)
        {
            stage.s_data(link.data);
            stage.s_tag(link.tag);
            stage.s_re(link.re);
            stage.s_im(link.im);
        }

        static void bind_m(Stage &stage, Link &link)
        {
            stage.m_data(link.data);
            stage.m_tag(link.tag);
            stage.m_re(link.re);
            stage.m_im(link.im);
        }

        static VBusValue read(const Link &link)
        {
            return {link.data.read(), static_cast<uint8_t>(link.tag.read()), static_cast<uint16_t>(link.re.read()),
                    static_cast<uint16_t>(link.im.read())};
        }

        static void write(Link &link, const VBusValue &value)
        {
            link.data.write(value.data);
            link.tag.write(value.tag);
            link.re.write(value.re);
            link.im.write(value.im);
        }
    };

    /// Kind "packed": one port and one signal of 64 bits.
    struct VBusVectorSC
    {
        using Stage = ::VBusPackedSC;

        struct Link
        {
            sc_core::sc_signal<uint64_t> bits;

            explicit Link(const std::string &name) : bits(name.c_str()) {}
        };

        static void bind_s(Stage &stage, Link &link)
        {
            stage.s(link.bits);
        }

        static void bind_m(Stage &stage, Link &link)
        {
            stage.m(link.bits);
        }

        static VBusValue read(const Link &link)
        {
            return vbus_unpack(link.bits.read());
        }

        static void write(Link &link, const VBusValue &value)
        {
            link.bits.write(vbus_pack(value));
        }
    };

    template <typename Kind>
    class VBusPipelineSC : public sc_core::sc_module
    {
    public:
        using Stage = typename Kind::Stage;
        using Link = typename Kind::Link;

        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<uint32_t> in{"in"};
        sc_core::sc_out<uint32_t> out{"out"};

        sc_core::sc_vector<Stage> stages;
        std::vector<std::unique_ptr<Link>> links;

        SC_HAS_PROCESS(VBusPipelineSC);

        VBusPipelineSC(sc_core::sc_module_name name, int count)
            : sc_core::sc_module(name), stages("stage", static_cast<std::size_t>(count))
        {
            for (int i = 0; i <= count; ++i)
            {
                links.push_back(std::make_unique<Link>("link" + std::to_string(i)));
            }
            for (std::size_t i = 0; i < stages.size(); ++i)
            {
                stages[i].clk(clk);
                Kind::bind_s(stages[i], *links[i]);
                Kind::bind_m(stages[i], *links[i + 1]);
            }

            SC_METHOD(source);
            sensitive << in;
            SC_METHOD(sink);
            sensitive << clk.pos();
            dont_initialize();
        }

        void source()
        {
            Kind::write(*links.front(), vbus_source(in.read()));
        }

        void sink()
        {
            out.write(vbus_sink(Kind::read(*links.back())));
        }
    };
}
