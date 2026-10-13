#include <dspsim/vmodule/vmodule.h>
#include <VStructArrayModel.h>
#include <VStructModel.h>

#include <array>
#include <cstdint>

// The struct ports of hdl/StructModel.sv and hdl/StructArrayModel.sv as values: one port with a struct as its
// value for each struct port, and an array of such ports for an array of structs.
//
// The structs below are what the generator writes for these models (verilator_value_struct.jinja), each
// with the functions that copy it to and from the verilated model. Their names end with a hash of their
// fields, which is what makes a struct type the same C++ type in every module.

#ifndef DSPSIM_GENERATED_iq_t_1ddb8578a0
#define DSPSIM_GENERATED_iq_t_1ddb8578a0
namespace dspsim::generated
{
    // Unpacked struct struct_model_pkg::iq_t
    struct iq_t_1ddb8578a0
    {
        int8_t re{};
        int8_t im{};

        bool operator==(const iq_t_1ddb8578a0 &) const = default;

        template <typename VlStruct>
        void to_model(VlStruct &vl_struct) const
        {
            ::dspsim::detail::vl_numbers_to_model<int8_t, 8>(vl_struct.__PVT__re, re);
            ::dspsim::detail::vl_numbers_to_model<int8_t, 8>(vl_struct.__PVT__im, im);
        }

        template <typename VlStruct>
        void from_model(const VlStruct &vl_struct)
        {
            ::dspsim::detail::vl_numbers_from_model<int8_t, 8>(vl_struct.__PVT__re, re);
            ::dspsim::detail::vl_numbers_from_model<int8_t, 8>(vl_struct.__PVT__im, im);
        }
    };
    DSPSIM_STRUCT(iq_t_1ddb8578a0, re, im)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_flags_t_eb47d5e788
#define DSPSIM_GENERATED_flags_t_eb47d5e788
namespace dspsim::generated
{
    // Packed struct struct_model_pkg::flags_t
    struct flags_t_eb47d5e788
    {
        uint8_t id{};
        uint8_t last{};

        bool operator==(const flags_t_eb47d5e788 &) const = default;

        template <typename VlWord>
        void to_bits(VlWord *vl_words, int vl_lsb) const
        {
            ::dspsim::detail::vl_number_to_bits<uint8_t, 3>(vl_words, vl_lsb + 1, id);
            ::dspsim::detail::vl_number_to_bits<uint8_t, 1>(vl_words, vl_lsb + 0, last);
        }

        template <typename VlWord>
        void from_bits(const VlWord *vl_words, int vl_lsb)
        {
            id = ::dspsim::detail::vl_number_from_bits<uint8_t, 3>(vl_words, vl_lsb + 1);
            last = ::dspsim::detail::vl_number_from_bits<uint8_t, 1>(vl_words, vl_lsb + 0);
        }
    };
    DSPSIM_STRUCT(flags_t_eb47d5e788, id, last)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_bus_t_17311acd62
#define DSPSIM_GENERATED_bus_t_17311acd62
namespace dspsim::generated
{
    // Unpacked struct struct_model_pkg::bus_t
    struct bus_t_17311acd62
    {
        int16_t data{};
        uint8_t valid{};
        std::array<uint8_t, 4> taps{};
        ::dspsim::generated::iq_t_1ddb8578a0 iq{};
        std::array<uint32_t, 4> wide{};
        ::dspsim::generated::flags_t_eb47d5e788 flags{};

        bool operator==(const bus_t_17311acd62 &) const = default;

        template <typename VlStruct>
        void to_model(VlStruct &vl_struct) const
        {
            ::dspsim::detail::vl_numbers_to_model<int16_t, 12>(vl_struct.__PVT__data, data);
            ::dspsim::detail::vl_numbers_to_model<uint8_t, 1>(vl_struct.__PVT__valid, valid);
            ::dspsim::detail::vl_numbers_to_model<uint8_t, 6>(vl_struct.__PVT__taps, taps);
            ::dspsim::detail::vl_struct_to_model(vl_struct.__PVT__iq, iq);
            ::dspsim::detail::vl_wide_to_model<100>(vl_struct.__PVT__wide, wide);
            ::dspsim::detail::vl_struct_to_model(vl_struct.__PVT__flags, flags);
        }

        template <typename VlStruct>
        void from_model(const VlStruct &vl_struct)
        {
            ::dspsim::detail::vl_numbers_from_model<int16_t, 12>(vl_struct.__PVT__data, data);
            ::dspsim::detail::vl_numbers_from_model<uint8_t, 1>(vl_struct.__PVT__valid, valid);
            ::dspsim::detail::vl_numbers_from_model<uint8_t, 6>(vl_struct.__PVT__taps, taps);
            ::dspsim::detail::vl_struct_from_model(vl_struct.__PVT__iq, iq);
            ::dspsim::detail::vl_wide_from_model<100>(vl_struct.__PVT__wide, wide);
            ::dspsim::detail::vl_struct_from_model(vl_struct.__PVT__flags, flags);
        }
    };
    DSPSIM_STRUCT(bus_t_17311acd62, data, valid, taps, iq, wide, flags)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_packed_bus_t_b6389d0e8e
#define DSPSIM_GENERATED_packed_bus_t_b6389d0e8e
namespace dspsim::generated
{
    // Packed struct struct_model_pkg::packed_bus_t
    struct packed_bus_t_b6389d0e8e
    {
        int16_t data{};
        uint8_t valid{};
        ::dspsim::generated::flags_t_eb47d5e788 flags{};

        bool operator==(const packed_bus_t_b6389d0e8e &) const = default;

        template <typename VlWord>
        void to_bits(VlWord *vl_words, int vl_lsb) const
        {
            ::dspsim::detail::vl_number_to_bits<int16_t, 12>(vl_words, vl_lsb + 5, data);
            ::dspsim::detail::vl_number_to_bits<uint8_t, 1>(vl_words, vl_lsb + 4, valid);
            flags.to_bits(vl_words, vl_lsb + 0);
        }

        template <typename VlWord>
        void from_bits(const VlWord *vl_words, int vl_lsb)
        {
            data = ::dspsim::detail::vl_number_from_bits<int16_t, 12>(vl_words, vl_lsb + 5);
            valid = ::dspsim::detail::vl_number_from_bits<uint8_t, 1>(vl_words, vl_lsb + 4);
            flags.from_bits(vl_words, vl_lsb + 0);
        }
    };
    DSPSIM_STRUCT(packed_bus_t_b6389d0e8e, data, valid, flags)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_packed_wide_t_44b8b84a9f
#define DSPSIM_GENERATED_packed_wide_t_44b8b84a9f
namespace dspsim::generated
{
    // Packed struct struct_model_pkg::packed_wide_t
    struct packed_wide_t_44b8b84a9f
    {
        std::array<uint32_t, 4> wide{};
        int64_t mid{};
        ::dspsim::generated::flags_t_eb47d5e788 flags{};
        uint8_t lo{};

        bool operator==(const packed_wide_t_44b8b84a9f &) const = default;

        template <typename VlWord>
        void to_bits(VlWord *vl_words, int vl_lsb) const
        {
            ::dspsim::detail::vl_wide_to_bits<100>(vl_words, vl_lsb + 47, wide);
            ::dspsim::detail::vl_number_to_bits<int64_t, 40>(vl_words, vl_lsb + 7, mid);
            flags.to_bits(vl_words, vl_lsb + 3);
            ::dspsim::detail::vl_number_to_bits<uint8_t, 3>(vl_words, vl_lsb + 0, lo);
        }

        template <typename VlWord>
        void from_bits(const VlWord *vl_words, int vl_lsb)
        {
            ::dspsim::detail::vl_wide_from_bits<100>(vl_words, vl_lsb + 47, wide);
            mid = ::dspsim::detail::vl_number_from_bits<int64_t, 40>(vl_words, vl_lsb + 7);
            flags.from_bits(vl_words, vl_lsb + 3);
            lo = ::dspsim::detail::vl_number_from_bits<uint8_t, 3>(vl_words, vl_lsb + 0);
        }
    };
    DSPSIM_STRUCT(packed_wide_t_44b8b84a9f, wide, mid, flags, lo)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_elem_t_2c41e6443a
#define DSPSIM_GENERATED_elem_t_2c41e6443a
namespace dspsim::generated
{
    // Unpacked struct struct_array_pkg::elem_t
    struct elem_t_2c41e6443a
    {
        int16_t data{};
        std::array<uint8_t, 2> taps{};
        ::dspsim::generated::iq_t_1ddb8578a0 iq{};
        std::array<uint32_t, 4> wide{};
        ::dspsim::generated::flags_t_eb47d5e788 flags{};

        bool operator==(const elem_t_2c41e6443a &) const = default;

        template <typename VlStruct>
        void to_model(VlStruct &vl_struct) const
        {
            ::dspsim::detail::vl_numbers_to_model<int16_t, 12>(vl_struct.__PVT__data, data);
            ::dspsim::detail::vl_numbers_to_model<uint8_t, 6>(vl_struct.__PVT__taps, taps);
            ::dspsim::detail::vl_struct_to_model(vl_struct.__PVT__iq, iq);
            ::dspsim::detail::vl_wide_to_model<100>(vl_struct.__PVT__wide, wide);
            ::dspsim::detail::vl_struct_to_model(vl_struct.__PVT__flags, flags);
        }

        template <typename VlStruct>
        void from_model(const VlStruct &vl_struct)
        {
            ::dspsim::detail::vl_numbers_from_model<int16_t, 12>(vl_struct.__PVT__data, data);
            ::dspsim::detail::vl_numbers_from_model<uint8_t, 6>(vl_struct.__PVT__taps, taps);
            ::dspsim::detail::vl_struct_from_model(vl_struct.__PVT__iq, iq);
            ::dspsim::detail::vl_wide_from_model<100>(vl_struct.__PVT__wide, wide);
            ::dspsim::detail::vl_struct_from_model(vl_struct.__PVT__flags, flags);
        }
    };
    DSPSIM_STRUCT(elem_t_2c41e6443a, data, taps, iq, wide, flags)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_packed_elem_t_a119db408d
#define DSPSIM_GENERATED_packed_elem_t_a119db408d
namespace dspsim::generated
{
    // Packed struct struct_array_pkg::packed_elem_t
    struct packed_elem_t_a119db408d
    {
        std::array<uint32_t, 3> wide{};
        int16_t data{};
        ::dspsim::generated::flags_t_eb47d5e788 flags{};

        bool operator==(const packed_elem_t_a119db408d &) const = default;

        template <typename VlWord>
        void to_bits(VlWord *vl_words, int vl_lsb) const
        {
            ::dspsim::detail::vl_wide_to_bits<70>(vl_words, vl_lsb + 16, wide);
            ::dspsim::detail::vl_number_to_bits<int16_t, 12>(vl_words, vl_lsb + 4, data);
            flags.to_bits(vl_words, vl_lsb + 0);
        }

        template <typename VlWord>
        void from_bits(const VlWord *vl_words, int vl_lsb)
        {
            ::dspsim::detail::vl_wide_from_bits<70>(vl_words, vl_lsb + 16, wide);
            data = ::dspsim::detail::vl_number_from_bits<int16_t, 12>(vl_words, vl_lsb + 4);
            flags.from_bits(vl_words, vl_lsb + 0);
        }
    };
    DSPSIM_STRUCT(packed_elem_t_a119db408d, wide, data, flags)
} // namespace dspsim::generated
#endif

#ifndef DSPSIM_GENERATED_outer_t_066f4eca17
#define DSPSIM_GENERATED_outer_t_066f4eca17
namespace dspsim::generated
{
    // Unpacked struct struct_array_pkg::outer_t
    struct outer_t_066f4eca17
    {
        std::array<::dspsim::generated::iq_t_1ddb8578a0, 3> iqs{};
        std::array<::dspsim::generated::flags_t_eb47d5e788, 2> flags{};
        uint8_t valid{};

        bool operator==(const outer_t_066f4eca17 &) const = default;

        template <typename VlStruct>
        void to_model(VlStruct &vl_struct) const
        {
            ::dspsim::detail::vl_structs_to_model<::dspsim::generated::iq_t_1ddb8578a0>(vl_struct.__PVT__iqs, iqs);
            ::dspsim::detail::vl_structs_to_model<::dspsim::generated::flags_t_eb47d5e788>(vl_struct.__PVT__flags, flags);
            ::dspsim::detail::vl_numbers_to_model<uint8_t, 1>(vl_struct.__PVT__valid, valid);
        }

        template <typename VlStruct>
        void from_model(const VlStruct &vl_struct)
        {
            ::dspsim::detail::vl_structs_from_model<::dspsim::generated::iq_t_1ddb8578a0>(vl_struct.__PVT__iqs, iqs);
            ::dspsim::detail::vl_structs_from_model<::dspsim::generated::flags_t_eb47d5e788>(vl_struct.__PVT__flags, flags);
            ::dspsim::detail::vl_numbers_from_model<uint8_t, 1>(vl_struct.__PVT__valid, valid);
        }
    };
    DSPSIM_STRUCT(outer_t_066f4eca17, iqs, flags, valid)
} // namespace dspsim::generated
#endif

namespace value_model
{
    using Bus = dspsim::generated::bus_t_17311acd62;
    using PackedBus = dspsim::generated::packed_bus_t_b6389d0e8e;
    using PackedWide = dspsim::generated::packed_wide_t_44b8b84a9f;
    using Flags = dspsim::generated::flags_t_eb47d5e788;
    using Iq = dspsim::generated::iq_t_1ddb8578a0;
    using Elem = dspsim::generated::elem_t_2c41e6443a;
    using PackedElem = dspsim::generated::packed_elem_t_a119db408d;
    using Outer = dspsim::generated::outer_t_066f4eca17;
}

// The modules use VModule::eval(), which copies the ports through their virtual sync().
DSPSIM_VMOD(StructValueModel, VStructModel)
{
public:
    DSPSIM_VINPUT(clk, 1);
    DSPSIM_VINPUT_STRUCT(u_in, value_model::Bus);
    DSPSIM_VOUTPUT_STRUCT(u_out, value_model::Bus);
    DSPSIM_VINPUT_STRUCT(p_in, value_model::PackedBus);
    DSPSIM_VOUTPUT_STRUCT(p_out, value_model::PackedBus);
    DSPSIM_VINPUT_STRUCT(w_in, value_model::PackedWide);
    DSPSIM_VOUTPUT_STRUCT(w_out, value_model::PackedWide);

    DSPSIM_CTOR(StructValueModel)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }
};

DSPSIM_VMOD(StructArrayValueModel, VStructArrayModel)
{
public:
    DSPSIM_VINPUT(clk, 1);
    // Unpacked structs, packed structs in one integer each, and packed structs of three words each.
    DSPSIM_VINPUT_STRUCT_ARRAY(a_in, value_model::Elem, 2, 3);
    DSPSIM_VOUTPUT_STRUCT_ARRAY(a_out, value_model::Elem, 2, 3);
    DSPSIM_VINPUT_STRUCT_ARRAY(f_in, value_model::Flags, 4);
    DSPSIM_VOUTPUT_STRUCT_ARRAY(f_out, value_model::Flags, 4);
    DSPSIM_VINPUT_STRUCT_ARRAY(w_in, value_model::PackedElem, 2);
    DSPSIM_VOUTPUT_STRUCT_ARRAY(w_out, value_model::PackedElem, 2);
    // A struct with arrays of structs inside.
    DSPSIM_VINPUT_STRUCT(o_in, value_model::Outer);
    DSPSIM_VOUTPUT_STRUCT(o_out, value_model::Outer);
    DSPSIM_VINPUT(u_in, 16);
    DSPSIM_VOUTPUT(u_out, 16);
    DSPSIM_VINPUT(pk_in, 12);
    DSPSIM_VOUTPUT(pk_out, 12);

    DSPSIM_CTOR(StructArrayValueModel)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }
};
