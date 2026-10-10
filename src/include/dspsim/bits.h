#pragma once
#include <climits>
#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>

/*
    Bit manipulation helpers for working with sliced and packed signals.
    Slices are unsigned (as in SystemVerilog); use sext() to interpret a slice as a signed value.

    Each helper has a runtime-width version (used by the Python bindings) and, where it pays off,
    a compile-time-width template version. Widths are in bits, 1..64 (mask() also accepts 0).
*/
namespace dspsim
{
    /// Mask selecting the lower width bits. width must be in [0, 64].
    constexpr uint64_t mask(int width)
    {
        return width >= 64 ? ~uint64_t{0} : (uint64_t{1} << width) - 1;
    }

    /// Mask selecting the lower W bits of the integral type U.
    template <int W, std::integral U = uint64_t>
    constexpr U mask()
    {
        static_assert(W >= 0, "Mask width must not be negative.");
        if constexpr (W >= static_cast<int>(sizeof(U) * CHAR_BIT))
        {
            return static_cast<U>(~U{0});
        }
        else
        {
            return static_cast<U>((U{1} << W) - 1);
        }
    }

    /// Throw std::out_of_range unless 0 <= lo <= hi < width. what names the thing being selected for the message.
    void check_bit_range(int hi, int lo, int width, const std::string &what);

    /// Extract bits [hi:lo] of value, shifted down to bit 0. Requires 0 <= lo <= hi < 64.
    constexpr uint64_t bits(uint64_t value, int hi, int lo)
    {
        return (value >> lo) & mask(hi - lo + 1);
    }

    /// Zero extend the lower width bits of value (clear everything above them).
    constexpr uint64_t zext(uint64_t value, int width)
    {
        return value & mask(width);
    }

    /// Sign extend the lower width bits of value from bit width-1. width must be in [1, 64].
    constexpr int64_t sext(uint64_t value, int width)
    {
        const int shift = 64 - width;
        return static_cast<int64_t>(value << shift) >> shift;
    }

    /// Sign extend the lower W bits of value, returning the signed type of the same size as T.
    template <int W, std::integral T>
    constexpr std::make_signed_t<T> sext(T value)
    {
        using S = std::make_signed_t<T>;
        static_assert(W > 0 && W <= static_cast<int>(sizeof(T) * CHAR_BIT), "Sign extension width out of range.");
        if constexpr (W == static_cast<int>(sizeof(T) * CHAR_BIT))
        {
            return static_cast<S>(value);
        }
        else
        {
            struct
            {
                S v : W;
            } field{};
            field.v = static_cast<S>(value);
            return field.v;
        }
    }
} // namespace dspsim
