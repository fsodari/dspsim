#pragma once
#include <dspsim/model.h>
#include <array>
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <iterator>

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

    template <typename UT>
    struct StdintSignedMap;
    template <>
    struct StdintSignedMap<uint8_t>
    {
        using type = int8_t;
    };
    template <>
    struct StdintSignedMap<uint16_t>
    {
        using type = int16_t;
    };
    template <>
    struct StdintSignedMap<uint32_t>
    {
        using type = int32_t;
    };
    template <>
    struct StdintSignedMap<uint64_t>
    {
        using type = int64_t;
    };

    template <typename T>
    inline T _sign_extend(T value, T sign_bit, T sign_mask)
    {
        return (value & sign_mask) - (value & sign_bit);
    }
    template <typename T>
    inline T sign_extend(T value, int width)
    {
        T sign_bit = 1 << (width - 1);
        T sign_mask = sign_bit - 1;
        return _sign_extend(value, sign_bit, sign_mask);
    }

    template <typename T>
    inline std::vector<T> sign_extend(std::vector<T> &data, int width)
    {
        T sign_bit = 1 << (width - 1);
        T sign_mask = sign_bit - 1;
        std::vector<T> result;
        result.reserve(data.size());

        std::transform(data.begin(), data.end(), std::back_inserter(result), [&sign_bit, &sign_mask](const T &x)
                       { return _sign_extend(x, sign_bit, sign_mask); });
        return result;
    }

    template <typename T>
    inline std::vector<double> sign_extendf(std::vector<T> &data, int width, int q)
    {
        T sign_bit = 1 << (width - 1);
        T sign_mask = sign_bit - 1;
        std::vector<T> result;
        result.reserve(data.size());

        const double sf = std::pow(2, q);

        std::transform(data.begin(), data.end(), std::back_inserter(result), [&sf, &sign_bit, &sign_mask](const T &x)
                       { return sf * static_cast<StdintSignedMap<T>::type>(_sign_extend(x, sign_bit, sign_mask)); });
        return result;
    }

    template <typename T>
    class Signal : public Model
    {
    public:
        Signal(T init = 0, int width = default_bitwidth<T>::value, bool sign_ext = false);

        virtual void eval_step() {}
        virtual void eval_end_step();

        void set_width(int width);
        int get_width() const { return m_width; }
        int width() const { return get_width(); }

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

        void _bind(T *other)
        {
            d = other;
        }

        static std::shared_ptr<Signal<T>> create(T init = 0, int width = default_bitwidth<T>::value, bool sign_ext = false)
        {
            return Model::create<Signal<T>>(init, width, sign_ext);
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

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

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
        Dff(Signal<uint8_t> &clk, T initial = 0, int width = default_bitwidth<T>::value, bool sign_ext = false);

        virtual void eval_step();
        virtual void eval_end_step();

        // Signal interface
        // Implicit cast.
        operator const T() const;
        // assignment
        Signal<T> &operator=(const T &other);
        Signal<T> &operator=(const Signal<T> &other);

        static std::shared_ptr<Dff<T>> create(Signal<uint8_t> &clk, T initial = 0, int width = default_bitwidth<T>::value, bool sign_ext = false)
        {
            return Model::create<Dff<T>>(clk, initial, width, sign_ext);
        }
    };
} // namespace dspsim
