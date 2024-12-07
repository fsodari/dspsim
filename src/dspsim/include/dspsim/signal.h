#pragma once
#include <dspsim/model.h>
#include <array>
#include <type_traits>
// #include <nanobind/nanobind.h>
// #include <nanobind/stl/array.h>
// #include <nanobind/ndarray.h>

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
        Signal(T init = 0)
        {
            d_local = init;
            d = &d_local;
            q = init;
            prev_q = !init; // !init ?
        }

        virtual void eval_step() {}
        void eval_end_step()
        {
            prev_q = q;
            q = *d;
        }

        bool changed() const
        {
            return q != prev_q;
        }
        bool posedge() const
        {
            return q && !prev_q;
        }
        bool negedge() const
        {
            return !q && prev_q;
        }

        // Signal interface
        // Implicit cast.
        operator const T() const { return this->read(); }
        // explicit operator int32_t() const { return this->read(); }

        // assignment
        Signal<T> &operator=(const T &other)
        {
            this->write(other);
            return *this;
        }

        Signal<T> &operator=(const Signal<T> &other)
        {
            this->write(other.read());
            return *this;
        }

        void write(T value)
        {
            *d = value;
        }
        T read() const
        {
            return q;
        }

        //
        T _read_d() const
        {
            return *d;
        }

        //
        void _force(T value)
        {
            *d = value;
            q = value;
        }
        void _bind(T &other)
        {
            d = &other;
        }

        int _readi() const
        {
            // Sign extend?
            return (int)q;
        }
        long _readl() const
        {
            // Sign extend?
            return (long)q;
        }

        // static std::shared_ptr<Signal<T>> create(T init = 0)
        // {
        //     return create<Signal<T>>(init);
        // }

    protected:
        T d_local;
        T *d, q;
        T prev_q;
    };

    // template <typename T, size_t N>
    // class Signal<T[N]>
    // {
    // public:
    //     auto read() const
    //     {
    //         // return new q;
    //         return std::array<T, N>(this->q);
    //     }

    //     //
    //     auto _read_d() const
    //     {
    //         // return new d;
    //         return std::array<T, N>(this->d);
    //     }
    // };

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
    template <typename T, size_t... Shape>
    struct signal_array
    {
        // if (std::enable_if<)
        //     using type = std::array<std::array<std::array<Signal<T> *, N3>, N2>, N1>;
    };

    // template <typename T, int N1, int N2>
    // struct signal_array<T, N1, N2, -1>
    // {
    //     using type = std::array<std::array<Signal<T> *, N2>, N1>;
    // };

    // template <typename T, int N>
    // struct signal_array<T, N, -1, -1>
    // {
    //     using type = std::array<Signal<T> *, N>;
    // };

    // template <typename T, size_t... Shape>
    // using SignalArray = nanobind::ndarray<Signal<T> *, nanobind::numpy, nanobind::shape<(Shape)...>>;

    // template <typename T, size_t...Shape>
    // using SignalArray = std::array<

    template <typename T>
    class Dff : public Signal<T>
    {
    protected:
        Signal<uint8_t> &clk;
        bool update = false;

    public:
        Dff(Signal<uint8_t> &clk, T initial = 0) : Signal<T>(initial), clk(clk)
        {
        }

        void eval_step()
        {
            update = clk.posedge();
        }
        void eval_end_step()
        {
            this->prev_q = this->q;
            if (update)
            {
                this->q = this->_read_d();
            }
        }

        // Signal interface
        // Implicit cast.
        operator const T() const { return this->read(); }
        // explicit operator int32_t() const { return this->read(); }

        // assignment
        Signal<T> &operator=(const T &other)
        {
            this->write(other);
            return *this;
        }

        Signal<T> &operator=(const Signal<T> &other)
        {
            this->write(other.read());
            return *this;
        }
    };
} // namespace dspsim
