/*
    dspsim models for the verilated benchmarks: the modules of hdl/Bus.sv as dspsim modules, declared the way
    the generator declares them (verilator_module.h.jinja, verilator_struct.jinja), eval() included, and a pipeline of them.
    verilated_sc.h has the SystemC counterparts, which are the SystemC models that Verilator writes.

        in -> source -> link 0 -> stage 0 -> link 1 -> ... -> link n -> sink -> out

    The stages are verilated. The source (combinational) and the sink (clocked) are processes of the pipeline.
*/
#pragma once
#include "verilated_values.h"

#include <dspsim/dspsim.h>
#include <dspsim/struct.h>
#include <dspsim/vmodule/vmodule.h>

#include <VBusPacked.h>
#include <VBusPorts.h>
#include <VBusUnpacked.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    namespace bus_model
    {
        struct In
        {
            template <typename T, int W>
            using Port = dspsim::VInput<T, W>;
            template <typename T, int W, typename S>
            using Bits = dspsim::VInputBits<T, W, S>;
        };

        struct Out
        {
            template <typename T, int W>
            using Port = dspsim::VOutput<T, W>;
            template <typename T, int W, typename S>
            using Bits = dspsim::VOutputBits<T, W, S>;
        };

        // iq_t, packed: 24 bits of the 64-bit vector of the port.
        template <typename D>
        struct PackedIq : dspsim::PortStruct
        {
            template <typename E>
            PackedIq(const std::string &name, E &ext, int lsb)
                : PortStruct(name), re(name + ".re", ext, lsb + 12), im(name + ".im", ext, lsb)
            {
                add("re", re);
                add("im", im);
            }

            void sync()
            {
                dspsim::detail::vl_sync(re);
                dspsim::detail::vl_sync(im);
            }

            typename D::template Bits<int16_t, 12, uint64_t> re;
            typename D::template Bits<int16_t, 12, uint64_t> im;
        };

        // bus_t, packed: 64 bits in one integer.
        template <typename D>
        struct PackedBus : dspsim::PortStruct
        {
            template <typename E>
            PackedBus(const std::string &name, E &ext, int lsb = 0)
                : PortStruct(name), data(name + ".data", ext, lsb + 32), tag(name + ".tag", ext, lsb + 24), iq(name + ".iq", ext, lsb)
            {
                add("data", data);
                add("tag", tag);
                add("iq", iq);
            }

            void sync()
            {
                dspsim::detail::vl_sync(data);
                dspsim::detail::vl_sync(tag);
                dspsim::detail::vl_sync(iq);
            }

            typename D::template Bits<uint32_t, 32, uint64_t> data;
            typename D::template Bits<uint8_t, 8, uint64_t> tag;
            PackedIq<D> iq;
        };

        // unpacked_iq_t.
        template <typename D>
        struct UnpackedIq : dspsim::PortStruct
        {
            template <typename E>
            UnpackedIq(const std::string &name, E &ext)
                : PortStruct(name), re(name + ".re", ext.__PVT__re), im(name + ".im", ext.__PVT__im)
            {
                add("re", re);
                add("im", im);
            }

            void sync()
            {
                dspsim::detail::vl_sync(re);
                dspsim::detail::vl_sync(im);
            }

            typename D::template Port<int16_t, 12> re;
            typename D::template Port<int16_t, 12> im;
        };

        // unpacked_bus_t: every field is a variable of the verilated struct.
        template <typename D>
        struct UnpackedBus : dspsim::PortStruct
        {
            template <typename E>
            UnpackedBus(const std::string &name, E &ext)
                : PortStruct(name), data(name + ".data", ext.__PVT__data), tag(name + ".tag", ext.__PVT__tag), iq(name + ".iq", ext.__PVT__iq)
            {
                add("data", data);
                add("tag", tag);
                add("iq", iq);
            }

            void sync()
            {
                dspsim::detail::vl_sync(data);
                dspsim::detail::vl_sync(tag);
                dspsim::detail::vl_sync(iq);
            }

            typename D::template Port<uint32_t, 32> data;
            typename D::template Port<uint8_t, 8> tag;
            UnpackedIq<D> iq;
        };
    } // namespace bus_model

    /// BusPorts: a port per field.
    DSPSIM_VMOD(BusPortsModel, VBusPorts)
    {
    public:
        DSPSIM_VINPUT(clk, 1);
        DSPSIM_VINPUT(s_data, 32);
        DSPSIM_VINPUT(s_tag, 8);
        DSPSIM_VINPUT_S(s_re, 12);
        DSPSIM_VINPUT_S(s_im, 12);
        DSPSIM_VOUTPUT(m_data, 32);
        DSPSIM_VOUTPUT(m_tag, 8);
        DSPSIM_VOUTPUT_S(m_re, 12);
        DSPSIM_VOUTPUT_S(m_im, 12);

        DSPSIM_CTOR(BusPortsModel)
        {
            DSPSIM_METHOD(eval)
                ->always("*");
        }

        void eval()
        {
            dspsim::detail::vl_sync(clk);
            dspsim::detail::vl_sync(s_data);
            dspsim::detail::vl_sync(s_tag);
            dspsim::detail::vl_sync(s_re);
            dspsim::detail::vl_sync(s_im);
            top->eval();
            dspsim::detail::vl_sync(m_data);
            dspsim::detail::vl_sync(m_tag);
            dspsim::detail::vl_sync(m_re);
            dspsim::detail::vl_sync(m_im);
        }
    };

    /// BusPacked: a packed struct, whose fields are bits of one integer of the model.
    DSPSIM_VMOD(BusPackedModel, VBusPacked)
    {
    public:
        DSPSIM_VINPUT(clk, 1);
        bus_model::PackedBus<bus_model::In> s{"s", top->s};
        bus_model::PackedBus<bus_model::Out> m{"m", top->m};

        DSPSIM_CTOR(BusPackedModel)
        {
            DSPSIM_METHOD(eval)
                ->always("*");
        }

        void eval()
        {
            dspsim::detail::vl_sync(clk);
            dspsim::detail::vl_sync(s);
            top->eval();
            dspsim::detail::vl_sync(m);
        }
    };

    /// BusUnpacked: an unpacked struct, whose fields are variables of the model.
    DSPSIM_VMOD(BusUnpackedModel, VBusUnpacked)
    {
    public:
        DSPSIM_VINPUT(clk, 1);
        bus_model::UnpackedBus<bus_model::In> s{"s", top->s};
        bus_model::UnpackedBus<bus_model::Out> m{"m", top->m};

        DSPSIM_CTOR(BusUnpackedModel)
        {
            DSPSIM_METHOD(eval)
                ->always("*");
        }

        void eval()
        {
            dspsim::detail::vl_sync(clk);
            dspsim::detail::vl_sync(s);
            top->eval();
            dspsim::detail::vl_sync(m);
        }
    };

    /// The signals of a link, which the context owns.
    struct VBusSignals
    {
        dspsim::Signal<uint32_t> &data;
        dspsim::Signal<uint8_t> &tag;
        dspsim::Signal<int16_t> &re;
        dspsim::Signal<int16_t> &im;

        /// A signal per field.
        explicit VBusSignals(const std::string &name)
            : data(*dspsim::Signal<uint32_t>::create(name + ".data")),
              tag(*dspsim::Signal<uint8_t>::create(name + ".tag")),
              re(*dspsim::Signal<int16_t>::create(name + ".iq.re", 12)),
              im(*dspsim::Signal<int16_t>::create(name + ".iq.im", 12))
        {
        }

        /// The signals that a struct port created.
        explicit VBusSignals(const dspsim::SignalStruct &signals)
            : data(signals.signal<uint32_t>("data")),
              tag(signals.signal<uint8_t>("tag")),
              re(signals.nested("iq").signal<int16_t>("re")),
              im(signals.nested("iq").signal<int16_t>("im"))
        {
        }

        VBusValue read() const
        {
            return {data.read(), tag.read(), bits12(re.read()), bits12(im.read())};
        }

        void write(const VBusValue &value)
        {
            data.write(value.data);
            tag.write(value.tag);
            re.write(signed12(value.re));
            im.write(signed12(value.im));
        }
    };

    /*
        A kind has the type Stage and link(name, driver, reader), which creates the signals of a link and binds
        the output of one stage and the input of the next to them. Either stage can be missing.
    */
    /// Kind "ports": a port per field, each bound to a signal of its own.
    struct VBusFields
    {
        using Stage = BusPortsModel;

        static VBusSignals link(const std::string &name, Stage *driver, Stage *reader)
        {
            VBusSignals signals{name};
            if (driver)
            {
                driver->m_data.bind(signals.data);
                driver->m_tag.bind(signals.tag);
                driver->m_re.bind(signals.re);
                driver->m_im.bind(signals.im);
            }
            if (reader)
            {
                reader->s_data.bind(signals.data);
                reader->s_tag.bind(signals.tag);
                reader->s_re.bind(signals.re);
                reader->s_im.bind(signals.im);
            }
            return signals;
        }
    };

    /// Kinds "packed" and "unpacked": a struct port, bound as a whole to the signals that it creates.
    template <typename M>
    struct VBusStruct
    {
        using Stage = M;

        static VBusSignals link(const std::string &name, Stage *driver, Stage *reader)
        {
            const dspsim::PortStruct &ports = driver ? static_cast<const dspsim::PortStruct &>(driver->m) : reader->s;
            auto signals = ports.signals(name);
            if (driver)
            {
                driver->m.bind(signals);
            }
            if (reader)
            {
                reader->s.bind(signals);
            }
            return VBusSignals{signals};
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(VBusPipeline)
    {
    public:
        using Stage = typename Kind::Stage;

        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<uint32_t> in{"in"};
        dspsim::Output<uint32_t> out{"out"};

        std::vector<std::unique_ptr<Stage>> stages;
        std::vector<VBusSignals> links;

        VBusPipeline(::dspsim::ModuleName, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                stages.push_back(std::make_unique<Stage>("stage" + std::to_string(i)));
                stages.back()->clk.bind(clk);
            }
            for (int i = 0; i <= count; ++i)
            {
                Stage *driver = i > 0 ? stages[static_cast<std::size_t>(i) - 1].get() : nullptr;
                Stage *reader = i < count ? stages[static_cast<std::size_t>(i)].get() : nullptr;
                links.push_back(Kind::link("link" + std::to_string(i), driver, reader));
            }

            DSPSIM_METHOD(source)
                ->always(in);
            DSPSIM_METHOD(sink)
                ->always(clk.pos())
                ->initialize(false);
        }

        void source()
        {
            links.front().write(vbus_source(in.read()));
        }

        void sink()
        {
            out.write(vbus_sink(links.back().read()));
        }
    };
}
