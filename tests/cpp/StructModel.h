#include <dspsim/vmodule/vmodule.h>
#include <dspsim/struct.h>
#include <VStructModel.h>

// The struct ports of hdl/StructModel.sv, as the generator declares them (verilator_struct.jinja): one class per
// struct, with a port for each field. The classes here are templates over the direction because the model has
// an input and an output of every struct.
namespace struct_model
{
    struct In
    {
        template <typename T, int W>
        using Port = dspsim::VInput<T, W>;
        template <typename T, int W>
        using Array = dspsim::VInputArray<T, W>;
        template <int W>
        using Wide = dspsim::VInputWide<W>;
        template <typename T, int W, typename S>
        using Bits = dspsim::VInputBits<T, W, S>;
        template <int W>
        using WideBits = dspsim::VInputWideBits<W>;
    };

    struct Out
    {
        template <typename T, int W>
        using Port = dspsim::VOutput<T, W>;
        template <typename T, int W>
        using Array = dspsim::VOutputArray<T, W>;
        template <int W>
        using Wide = dspsim::VOutputWide<W>;
        template <typename T, int W, typename S>
        using Bits = dspsim::VOutputBits<T, W, S>;
        template <int W>
        using WideBits = dspsim::VOutputWideBits<W>;
    };

    // flags_t, packed: bits of a port whose words are of type S.
    template <typename D, typename S>
    struct Flags : dspsim::PortStruct
    {
        template <typename E>
        Flags(const std::string &name, E &ext, int lsb)
            : PortStruct(name), id(name + ".id", ext, lsb + 1), last(name + ".last", ext, lsb)
        {
            add("id", id);
            add("last", last);
        }

        typename D::template Bits<uint8_t, 3, S> id;
        typename D::template Bits<uint8_t, 1, S> last;
    };

    // iq_t, unpacked.
    template <typename D>
    struct Iq : dspsim::PortStruct
    {
        template <typename E>
        Iq(const std::string &name, E &ext)
            : PortStruct(name), re(name + ".re", ext.__PVT__re), im(name + ".im", ext.__PVT__im)
        {
            add("re", re);
            add("im", im);
        }

        typename D::template Port<int8_t, 8> re;
        typename D::template Port<int8_t, 8> im;
    };

    // bus_t, unpacked: every field is a variable of the verilated struct.
    template <typename D>
    struct Bus : dspsim::PortStruct
    {
        template <typename E>
        Bus(const std::string &name, E &ext)
            : PortStruct(name),
              data(name + ".data", ext.__PVT__data),
              valid(name + ".valid", ext.__PVT__valid),
              taps(name + ".taps", ext.__PVT__taps),
              iq(name + ".iq", ext.__PVT__iq),
              wide(name + ".wide", ext.__PVT__wide),
              flags(name + ".flags", ext.__PVT__flags, 0)
        {
            add("data", data);
            add("valid", valid);
            add("taps", taps);
            add("iq", iq);
            add("wide", wide);
            add("flags", flags);
        }

        typename D::template Port<int16_t, 12> data;
        typename D::template Port<uint8_t, 1> valid;
        typename D::template Array<uint8_t, 6> taps;
        Iq<D> iq;
        typename D::template Wide<100> wide;
        Flags<D, uint8_t> flags;
    };

    // packed_bus_t: 17 bits in one 32-bit integer.
    template <typename D>
    struct PackedBus : dspsim::PortStruct
    {
        template <typename E>
        PackedBus(const std::string &name, E &ext, int lsb = 0)
            : PortStruct(name), data(name + ".data", ext, lsb + 5), valid(name + ".valid", ext, lsb + 4), flags(name + ".flags", ext, lsb)
        {
            add("data", data);
            add("valid", valid);
            add("flags", flags);
        }

        typename D::template Bits<int16_t, 12, uint32_t> data;
        typename D::template Bits<uint8_t, 1, uint32_t> valid;
        Flags<D, uint32_t> flags;
    };

    // packed_wide_t: 147 bits in 32-bit words.
    template <typename D>
    struct PackedWide : dspsim::PortStruct
    {
        template <typename E>
        PackedWide(const std::string &name, E &ext, int lsb = 0)
            : PortStruct(name), wide(name + ".wide", ext, lsb + 47), mid(name + ".mid", ext, lsb + 7), flags(name + ".flags", ext, lsb + 3), lo(name + ".lo", ext, lsb)
        {
            add("wide", wide);
            add("mid", mid);
            add("flags", flags);
            add("lo", lo);
        }

        typename D::template WideBits<100> wide;
        typename D::template Bits<int64_t, 40, uint32_t> mid;
        Flags<D, uint32_t> flags;
        typename D::template Bits<uint8_t, 3, uint32_t> lo;
    };
} // namespace struct_model

DSPSIM_VMOD(StructModel, VStructModel)
{
public:
    // Ports.
    DSPSIM_VINPUT(clk, 1);
    struct_model::Bus<struct_model::In> u_in{"u_in", top->u_in};
    struct_model::Bus<struct_model::Out> u_out{"u_out", top->u_out};
    struct_model::PackedBus<struct_model::In> p_in{"p_in", top->p_in};
    struct_model::PackedBus<struct_model::Out> p_out{"p_out", top->p_out};
    struct_model::PackedWide<struct_model::In> w_in{"w_in", top->w_in};
    struct_model::PackedWide<struct_model::Out> w_out{"w_out", top->w_out};

    DSPSIM_CTOR(StructModel)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }
};
