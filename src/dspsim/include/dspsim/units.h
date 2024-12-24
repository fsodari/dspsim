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
    } // namespace units

} // namespace dspsim
