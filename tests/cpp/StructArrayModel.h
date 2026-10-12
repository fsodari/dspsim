#include <dspsim/vmodule/vmodule.h>
#include <dspsim/struct.h>
#include <VStructArrayModel.h>

// The struct ports of hdl/StructArrayModel.sv, as the generator declares them (verilator_struct.jinja). An array
// of structs is a PortStructArrayOf of the class of its structs, each constructed from one verilated struct.
// The classes here are templates over the direction, like those in StructModel.h.
namespace struct_array_model
{
    using dspsim::PortStructArrayOf;
    using dspsim::Shape;
    using dspsim::detail::vl_element_pointers;
    using dspsim::detail::vl_structs;
    using dspsim::detail::vl_word_pointers;

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

    // flags_t, packed: bits of a vector whose words are of type S. In an array of them, ext is a pointer to the
    // first word of the vector.
    template <typename D, typename S>
    struct Flags : dspsim::PortStruct
    {
        template <typename E>
        Flags(const std::string &name, E &ext, int lsb = 0)
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

    // elem_t, unpacked: the element of a_in and a_out.
    template <typename D>
    struct Elem : dspsim::PortStruct
    {
        template <typename E>
        Elem(const std::string &name, E &ext)
            : PortStruct(name),
              data(name + ".data", ext.__PVT__data),
              taps(name + ".taps", ext.__PVT__taps),
              iq(name + ".iq", ext.__PVT__iq),
              wide(name + ".wide", ext.__PVT__wide),
              flags(name + ".flags", ext.__PVT__flags)
        {
            add("data", data);
            add("taps", taps);
            add("iq", iq);
            add("wide", wide);
            add("flags", flags);
        }

        typename D::template Port<int16_t, 12> data;
        typename D::template Array<uint8_t, 6> taps;
        Iq<D> iq;
        typename D::template Wide<100> wide;
        Flags<D, uint8_t> flags;
    };

    // packed_elem_t: 86 bits in three 32-bit words.
    template <typename D>
    struct PackedElem : dspsim::PortStruct
    {
        template <typename E>
        PackedElem(const std::string &name, E &ext, int lsb = 0)
            : PortStruct(name), wide(name + ".wide", ext, lsb + 16), data(name + ".data", ext, lsb + 4), flags(name + ".flags", ext, lsb)
        {
            add("wide", wide);
            add("data", data);
            add("flags", flags);
        }

        typename D::template WideBits<70> wide;
        typename D::template Bits<int16_t, 12, uint32_t> data;
        Flags<D, uint32_t> flags;
    };

    // outer_t: a struct with arrays of structs as fields.
    template <typename D>
    struct Outer : dspsim::PortStruct
    {
        template <typename E>
        Outer(const std::string &name, E &ext)
            : PortStruct(name),
              iqs(name + ".iqs", Shape{3}, vl_structs<Iq<D>>(vl_element_pointers(ext.__PVT__iqs))),
              flags(name + ".flags", Shape{2}, vl_structs<Flags<D, uint8_t>>(vl_word_pointers<uint8_t>(ext.__PVT__flags, 2))),
              valid(name + ".valid", ext.__PVT__valid)
        {
            add("iqs", iqs);
            add("flags", flags);
            add("valid", valid);
        }

        PortStructArrayOf<Iq<D>> iqs;
        PortStructArrayOf<Flags<D, uint8_t>> flags;
        typename D::template Port<uint8_t, 1> valid;
    };
} // namespace struct_array_model

DSPSIM_VMOD(StructArrayModel, VStructArrayModel)
{
    using In = struct_array_model::In;
    using Out = struct_array_model::Out;
    template <typename S>
    using Structs = dspsim::PortStructArrayOf<S>;

public:
    // Ports.
    DSPSIM_VINPUT(clk, 1);
    Structs<struct_array_model::Elem<In>> a_in{"a_in", {2, 3}, dspsim::detail::vl_structs<struct_array_model::Elem<In>>(dspsim::detail::vl_element_pointers(top->a_in))};
    Structs<struct_array_model::Elem<Out>> a_out{"a_out", {2, 3}, dspsim::detail::vl_structs<struct_array_model::Elem<Out>>(dspsim::detail::vl_element_pointers(top->a_out))};
    Structs<struct_array_model::Flags<In, uint8_t>> f_in{"f_in", {4}, dspsim::detail::vl_structs<struct_array_model::Flags<In, uint8_t>>(dspsim::detail::vl_word_pointers<uint8_t>(top->f_in, 4))};
    Structs<struct_array_model::Flags<Out, uint8_t>> f_out{"f_out", {4}, dspsim::detail::vl_structs<struct_array_model::Flags<Out, uint8_t>>(dspsim::detail::vl_word_pointers<uint8_t>(top->f_out, 4))};
    Structs<struct_array_model::PackedElem<In>> w_in{"w_in", {2}, dspsim::detail::vl_structs<struct_array_model::PackedElem<In>>(dspsim::detail::vl_word_pointers<uint32_t>(top->w_in, 2))};
    Structs<struct_array_model::PackedElem<Out>> w_out{"w_out", {2}, dspsim::detail::vl_structs<struct_array_model::PackedElem<Out>>(dspsim::detail::vl_word_pointers<uint32_t>(top->w_out, 2))};
    struct_array_model::Outer<In> o_in{"o_in", top->o_in};
    struct_array_model::Outer<Out> o_out{"o_out", top->o_out};
    // A packed union and a packed array of structs are the vectors of their bits.
    DSPSIM_VINPUT(u_in, 16);
    DSPSIM_VOUTPUT(u_out, 16);
    DSPSIM_VINPUT(pk_in, 12);
    DSPSIM_VOUTPUT(pk_out, 12);

    DSPSIM_CTOR(StructArrayModel)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }
};
