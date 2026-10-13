/*
    What the testbench of the verilated benchmarks computes, for both engines: the value of a bus, what the
    source makes of the input and the sink of the last bus. The stages are verilated (hdl/Bus.sv).

    This file depends on nothing but the standard library.
*/
#pragma once
#include <cstdint>

namespace benchmarks
{
    /// The fields of bus_t in hdl/Bus.sv. re and im are the 12 bits of the signed fields, as unsigned numbers.
    struct VBusValue
    {
        uint32_t data = 0;
        uint8_t tag = 0;
        uint16_t re = 0;
        uint16_t im = 0;
    };

    /// The 12 bits of a signed field as a signed number, and back.
    inline int16_t signed12(uint16_t bits)
    {
        return static_cast<int16_t>((bits & 0xFFF) ^ 0x800) - 0x800;
    }

    inline uint16_t bits12(int16_t value)
    {
        return static_cast<uint16_t>(value) & 0xFFF;
    }

    /// The bus that the source makes of the input of the testbench.
    inline VBusValue vbus_source(uint32_t in)
    {
        return {in, static_cast<uint8_t>(in >> 8), static_cast<uint16_t>((in >> 16) & 0xFFF), static_cast<uint16_t>((in >> 3) & 0xFFF)};
    }

    /// The output that the sink makes of the last bus.
    inline uint32_t vbus_sink(const VBusValue &v)
    {
        return v.data ^ (static_cast<uint32_t>(v.tag) << 24) ^ v.re ^ (static_cast<uint32_t>(v.im) << 12);
    }

    /// The packed struct as one vector, with the first field in the most significant bits.
    inline uint64_t vbus_pack(const VBusValue &v)
    {
        return (static_cast<uint64_t>(v.data) << 32) | (static_cast<uint64_t>(v.tag) << 24) |
               (static_cast<uint64_t>(v.re) << 12) | v.im;
    }

    inline VBusValue vbus_unpack(uint64_t bits)
    {
        return {static_cast<uint32_t>(bits >> 32), static_cast<uint8_t>(bits >> 24), static_cast<uint16_t>((bits >> 12) & 0xFFF),
                static_cast<uint16_t>(bits & 0xFFF)};
    }
}
