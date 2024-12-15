#pragma once
#include <dspsim/model.h>
#include <array>
#include <type_traits>

namespace dspsim
{
    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    template <typename T>
    class Signal : public Model
    {
    public:
        Signal(T init = 0);

        virtual void eval_step() {}
        virtual void eval_end_step();

        bool changed() const { return q != prev_q; }
        bool posedge() const { return q && !prev_q; }
        bool negedge() const { return !q && prev_q; }

        // Signal interface
        // Implicit cast.
        operator const T() const;

        // assignment
        Signal<T> &operator=(const T &other)
        {
            this->write(other);
            return *this;
        }

        Signal<T> &operator=(const Signal<T> &other);

        void write(T value);
        T read() const;
        //
        T _read_d() const;

        //
        void _force(T value);
        void _bind(T &other);

        static std::shared_ptr<Signal<T>> create(T init = 0);

    protected:
        T d_local;
        T *d, q;
        T prev_q;
    };

    template <typename T>
    using SignalPtr = std::shared_ptr<Signal<T>>;

    template <typename T, size_t N>
    using SignalArray = std::array<SignalPtr<T>, N>;
    /*
    Scalar: Signal<T> sig;
    Array: std::array<SignalPtr<T>, N> arr;

    Array<T> arr = {Signal<T>::create(), Signal<T>::create(), Signal<T>::create()}
    2D Array: std::array<std::array<Signal<T> *, N2>, N1>

    */

    template <typename T>
    class Dff : public Signal<T>
    {
    protected:
        Signal<uint8_t> &clk;
        bool update = false;

    public:
        Dff(Signal<uint8_t> &clk, T initial = 0);

        virtual void eval_step();
        virtual void eval_end_step();

        // Signal interface
        // Implicit cast.
        operator const T() const;
        // assignment
        Signal<T> &operator=(const T &other);
        Signal<T> &operator=(const Signal<T> &other);

        static std::shared_ptr<Dff<T>> create(Signal<uint8_t> &clk, T initial = 0);
    };
} // namespace dspsim
