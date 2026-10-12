/*
    What the designs of the structs benchmarks compute, for both engines: the values that travel through the
    pipelines, and the functions of the source, a stage and the sink. structs.h and structs_sc.h only differ
    in how a value is read from the ports and written to them. The values are also the value types of the
    SystemC signals that carry a whole struct.

    This file depends on nothing but the standard library.
*/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace benchmarks
{
    /*
        The bus design: a struct with two fields and a nested struct.
    */
    struct IqValue
    {
        int16_t re = 0;
        int16_t im = 0;

        bool operator==(const IqValue &) const = default;
    };

    struct BusValue
    {
        uint32_t data = 0;
        uint8_t tag = 0;
        IqValue iq;

        bool operator==(const BusValue &) const = default;
    };

    inline std::ostream &operator<<(std::ostream &os, const BusValue &v)
    {
        return os << '{' << v.data << ' ' << static_cast<int>(v.tag) << ' ' << v.iq.re << ' ' << v.iq.im << '}';
    }

    /// The bus that the source makes of the input of the testbench.
    inline BusValue bus_source(uint32_t in)
    {
        return {in, static_cast<uint8_t>(in >> 8), {static_cast<int16_t>(in >> 16), static_cast<int16_t>(in >> 3)}};
    }

    /// What a stage registers. Every field changes, and depends on other fields.
    inline BusValue bus_stage(const BusValue &v)
    {
        return {v.data + v.tag, static_cast<uint8_t>(v.tag + 1), {v.iq.im, static_cast<int16_t>(v.iq.re + v.tag)}};
    }

    /// The output that the sink makes of the last bus.
    inline uint32_t bus_sink(const BusValue &v)
    {
        return v.data ^ (static_cast<uint32_t>(v.tag) << 24) ^ static_cast<uint16_t>(v.iq.re) ^
               (static_cast<uint32_t>(static_cast<uint16_t>(v.iq.im)) << 8);
    }

    /*
        The arrays design: K elements with two fields each, as arrays of the fields or as an array of structs.
    */
    struct ElemValue
    {
        uint32_t data = 0;
        uint8_t gain = 0;

        bool operator==(const ElemValue &) const = default;
    };

    inline std::ostream &operator<<(std::ostream &os, const ElemValue &v)
    {
        return os << '{' << v.data << ' ' << static_cast<int>(v.gain) << '}';
    }

    /// The elements as a struct of arrays.
    template <int K>
    struct SoaValue
    {
        std::array<uint32_t, K> data{};
        std::array<uint8_t, K> gain{};

        bool operator==(const SoaValue &) const = default;
    };

    template <int K>
    std::ostream &operator<<(std::ostream &os, const SoaValue<K> &v)
    {
        for (std::size_t i = 0; i < K; ++i)
        {
            os << ElemValue{v.data[i], v.gain[i]};
        }
        return os;
    }

    /// Element i that the source makes of the input of the testbench.
    inline ElemValue array_source(uint32_t in, std::size_t i)
    {
        return {in + static_cast<uint32_t>(i), static_cast<uint8_t>(in >> (i % 24))};
    }

    /// What a stage registers for one element.
    inline ElemValue array_stage(const ElemValue &v)
    {
        return {v.data + v.gain, static_cast<uint8_t>(v.gain + 1)};
    }

    /// The sink folds the elements of the last link into its output, starting from 0.
    inline uint32_t array_sink(uint32_t hash, const ElemValue &v)
    {
        return hash * 31 + (v.data ^ v.gain);
    }
}
