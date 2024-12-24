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

    template <typename T>
    int64_t sign_extend64(T value, int width)
    {
        T sign_bit = 1 << (width - 1);
        T mask0 = sign_bit - 1;

        return (value & mask0) - (value & sign_bit);
    }

    template <typename T>
    std::vector<T> sign_extend64(std::vector<T> &values, int width)
    {
        std::vector<T> result;

        T sign_bit = 1 << (width - 1);
        T mask0 = sign_bit - 1;

        std::transform(values.begin(), values.end(), std::back_inserter(result), [&sign_bit, &mask0](const T &x)
                       { return (x & mask0) - (x & sign_bit); });

        return result;
    }

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

        // Write a value to the d pin of the signal.
        Signal<T> &operator=(const T &other);

        // Write the q value of another signal to the d pin of this signal.
        Signal<T> &operator=(const Signal<T> &other);

        // Write to the d pin.
        void write(T value);
        // Read the q pin.
        T read() const;

        //
        T _read_d() const;

        //
        void _force(T value);
        void _bind(T &other);

        // Python is 64 bit so it tends to write with ints.
        void write64(int64_t value) { write(value); }
        int64_t read64() const { return sign_extend64(read(), default_bitwidth<T>::value); }

        static std::shared_ptr<Signal<T>> create(T init = 0);
        static std::shared_ptr<Signal<T>> create64(int64_t init = 0) { return Signal<T>::create(init); }

    protected:
        T d_local;
        T *d, q;
        T prev_q;
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
        static std::shared_ptr<Dff<T>> create64(Signal<uint8_t> &clk, int64_t initial = 0) { return Dff<T>::create(clk, initial); }
    };
} // namespace dspsim
