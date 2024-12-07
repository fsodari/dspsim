#pragma once
namespace dspsim
{
    namespace units
    {
        static constexpr double ps(double t) { return t * 1e-12; }
        static constexpr double ns(double t) { return t * 1e-9; }
        static constexpr double us(double t) { return t * 1e-6; }
        static constexpr double ms(double t) { return t * 1e-3; }
        static constexpr double s(double t) { return t; }

        constexpr double operator""_ps(long double t) { return ps(t); }
        constexpr double operator""_ns(long double t) { return ns(t); }
        constexpr double operator""_us(long double t) { return us(t); }
        constexpr double operator""_ms(long double t) { return ms(t); }
        constexpr double operator""_s(long double t) { return s(t); }

    } // namespace units

} // namespace dspsim
