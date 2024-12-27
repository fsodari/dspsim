#pragma once
#include <dspsim/model.h>
#include <array>
#include <type_traits>
#include <algorithm>
/*

def sign_extend(value: int, width: int) -> int:
    sign_bit = 1 << (width - 1)
    return (value & (sign_bit - 1)) - (value & sign_bit)


def sign_extendv(data: np.ndarray, width: int) -> int:
    sign_bit = 1 << (width - 1)
    mask0 = sign_bit - 1

    vxtnd = np.vectorize(lambda x: (x & mask0) - (x & sign_bit))

    return vxtnd(data)


*/
namespace dspsim
{
    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    // template <typename T>
    // int64_t sign_extend64(T value, int width)
    // {
    //     T sign_bit = 1 << (width - 1);
    //     T mask0 = sign_bit - 1;

    //     return (value & mask0) - (value & sign_bit);
    // }

    // template <typename T, typename IIter, typename OIter>
    // void sign_extend(int width, IIter ibegin, IIter iend, OIter obegin)
    // {
    //     const T sign_bit = 1 << (width - 1);
    //     const T mask0 = sign_bit - 1;

    //     std::transform(ibegin, iend, obegin, [&sign_bit, &mask0](const T &x)
    //                    { return (x & mask0) - (x & sign_bit); });
    // }
    template <typename T>
    inline T _sign_extend(T value, T sign_bit, T sign_mask)
    {
        return (value & sign_mask) - (value & sign_bit);
    }

    template <typename T>
    class Signal : public Model
    {
    public:
        Signal(T init = 0, int width = default_bitwidth<T>::value);

        virtual void eval_step() {}
        virtual void eval_end_step();

        void set_width(int width);
        int get_width() const { return m_width; }

        void set_sign_extend(bool extend) { m_extend = extend; }
        bool get_sign_extend() { return m_extend; }

        bool changed() const { return q != prev_q; }
        bool posedge() const { return q && !prev_q; }
        bool negedge() const { return !q && prev_q; }

        // Signal interface
        // Implicit cast.
        operator const T() const;

        // Write a value to the d pin of the signal.
        Signal<T> &operator=(const T &other);

        // Write the q value of another signal to the d pin of this signal.
        Signal<T> &operator=(const Signal<T> &other);

        // Write to the d pin.
        void write(T value);
        // Read the q pin. Optionally sign extended.
        T read() const;

        // Not sign extended.
        T _read_d() const;

        //
        void _force(T value);

        void _bind(T &other)
        {
            d = static_cast<T *>(&other);
        }

        // // // Python is 64 bit so it tends to write with ints.
        // // void write64(int64_t value) { write(value); }
        // // int64_t read64() const { return sign_extend64(read(), default_bitwidth<T>::value); }

        // template <typename PT>
        // void pywrite(PT value) { write(static_cast<T>(value)); }

        // template <typename PT>
        // PT pyread() const { return static_cast<PT>(read()); }

        static std::shared_ptr<Signal<T>> create(T init = 0, int width = default_bitwidth<T>::value)
        {
            return Model::create<Signal<T>>(init, width);
        }

    protected:
        T d_local;
        T *d, q;
        T prev_q;
        int m_width;
        T m_bitmask;
        T m_sign_bit;
        T m_sign_mask;
        bool m_extend = true;
    };

    template <typename T>
    using SignalPtr = std::shared_ptr<Signal<T>>;

    template <typename T, size_t N>
    using SignalArray = std::array<SignalPtr<T>, N>;

    template <typename T>
    class Dff : public Signal<T>
    {
    protected:
        Signal<uint8_t> &clk;
        bool update = false;

    public:
        Dff(Signal<uint8_t> &clk, T initial = 0, int width = default_bitwidth<T>::value);

        virtual void eval_step();
        virtual void eval_end_step();

        // Signal interface
        // Implicit cast.
        operator const T() const;
        // assignment
        Signal<T> &operator=(const T &other);
        Signal<T> &operator=(const Signal<T> &other);

        static std::shared_ptr<Dff<T>> create(Signal<uint8_t> &clk, T initial = 0, int width = default_bitwidth<T>::value);
    };
} // namespace dspsim
