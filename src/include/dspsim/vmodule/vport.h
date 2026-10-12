#pragma once
#include <dspsim/port.h>
#include <dspsim/bits.h>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dspsim
{
    namespace detail
    {
        /// Storage type of a verilated port for the dspsim port type T. Verilator stores integral ports
        /// (<= 64 bits) as unsigned stdint types of the same size, and real ports as double.
        template <typename T>
        using vl_storage_t = typename std::conditional_t<std::is_integral_v<T>, std::make_unsigned<T>, std::type_identity<T>>::type;

        /// Convert a W-bit value held in a verilated port into T. Signed types are sign extended from bit W-1.
        template <typename T, int W>
        constexpr T vl_extend(vl_storage_t<T> value)
        {
            if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
            {
                return sext<W>(value);
            }
            else
            {
                return static_cast<T>(value);
            }
        }

        /// Convert T into the verilated port's storage, clearing the bits above W. Verilator requires clean inputs.
        template <typename T, int W>
        constexpr vl_storage_t<T> vl_truncate(T value)
        {
            using U = vl_storage_t<T>;
            if constexpr (std::is_integral_v<T>)
            {
                return static_cast<U>(value) & mask<W, U>();
            }
            else
            {
                return value;
            }
        }

        /// Compile-time checks shared by the verilated port classes.
        template <typename T, int W>
        constexpr bool vl_port_check()
        {
            if constexpr (std::is_integral_v<T>)
            {
                static_assert(W > 0, "Verilated port width must be positive.");
                static_assert(W <= static_cast<int>(sizeof(T) * CHAR_BIT), "Verilated port width exceeds the port type.");
                static_assert(sizeof(T) == sizeof(vl_storage_t<T>), "Port type must match the verilated storage size.");
            }
            return true;
        }
    } // namespace detail

    /// Input port bound to a verilated model's input. T is the dspsim-side type (signed or unsigned) and W is
    /// the HDL port width. The value is truncated to W bits when written to the model.
    template <typename T, int W>
    class VInput final : public Input<T>
    {
        static_assert(detail::vl_port_check<T, W>());

    public:
        using Base = Input<T>;
        using Storage = detail::vl_storage_t<T>;

        VInput(const std::string &name, Storage &ext_port);

    private:
        /// Write the input value to the top model port.
        void sync() override;

    private:
        Storage *ext_port_;
    };

    /// Output port bound to a verilated model's output. Signed types are sign extended from the HDL port width W.
    template <typename T, int W>
    class VOutput final : public Output<T>
    {
        static_assert(detail::vl_port_check<T, W>());

    public:
        using Base = Output<T>;
        using Storage = detail::vl_storage_t<T>;

        VOutput(const std::string &name, Storage &ext_port);

    private:
        /// Write the top model value to the output port.
        void sync() override;

    private:
        Storage *ext_port_;
    };

    template <typename T, int W>
    VInput<T, W>::VInput(const std::string &name, Storage &ext_port) : Input<T>(name, W), ext_port_(&ext_port)
    {
    }

    template <typename T, int W>
    void VInput<T, W>::sync()
    {
        *ext_port_ = detail::vl_truncate<T, W>(this->read());
    }

    template <typename T, int W>
    VOutput<T, W>::VOutput(const std::string &name, Storage &ext_port) : Output<T>(name, W), ext_port_(&ext_port)
    {
    }

    template <typename T, int W>
    void VOutput<T, W>::sync()
    {
        this->write(detail::vl_extend<T, W>(*ext_port_));
    }

    namespace detail
    {
        // Verilated unpacked arrays (VlUnpacked) expose their contents as a C array named m_storage.
        template <typename A>
        concept VlArrayLike = requires { typename std::enable_if_t<std::is_array_v<decltype(A::m_storage)>>; };

        template <typename A>
        struct vl_element
        {
            using type = A;
        };
        template <VlArrayLike A>
        struct vl_element<A>
        {
            using type = typename vl_element<std::remove_extent_t<decltype(A::m_storage)>>::type;
        };

        /// Element storage type of a (possibly nested) verilated port.
        template <typename A>
        using vl_element_t = typename vl_element<std::remove_cvref_t<A>>::type;

        /// Signed dspsim type for a verilated port's element storage type.
        template <typename A>
        using vl_signed_t = std::make_signed_t<vl_element_t<A>>;

        // Row-major shape of a (nested) verilated array.
        template <typename A>
        void vl_shape(Shape &) {}
        template <VlArrayLike A>
        void vl_shape(Shape &shape)
        {
            using S = decltype(A::m_storage);
            shape.push_back(std::extent_v<S>);
            vl_shape<std::remove_extent_t<S>>(shape);
        }

        template <typename A>
        Shape vl_shape_of()
        {
            Shape shape;
            vl_shape<std::remove_cvref_t<A>>(shape);
            return shape;
        }

        /// The elements of a (nested) verilated port as one array, in row-major order. S is the element
        /// storage type, which must be the one of the port.
        template <typename S, typename A>
        S *vl_elements(A &ext_port)
        {
            static_assert(std::is_same_v<vl_element_t<A>, S>, "The port type does not match the storage of the verilated port.");
            return reinterpret_cast<S *>(&ext_port);
        }

        /// Pointers to the elements of a (nested) verilated port, in row-major order. The elements of an array
        /// of structs are the structs.
        template <typename A>
        std::vector<vl_element_t<A> *> vl_element_pointers(A &ext_port)
        {
            using S = vl_element_t<A>;
            std::vector<S *> pointers(sizeof(A) / sizeof(S));
            for (std::size_t i = 0; i < pointers.size(); ++i)
            {
                pointers[i] = vl_elements<S>(ext_port) + i;
            }
            return pointers;
        }

        /// Pointers to the first word of each of the `count` packed vectors that a verilated port consists of:
        /// an array of packed structs. S is the type of the words (the vector itself up to 64 bits, uint32_t
        /// above). Throws std::invalid_argument if the port does not divide into that many vectors.
        template <typename S, typename A>
        std::vector<S *> vl_word_pointers(A &ext_port, std::size_t count)
        {
            if (count == 0 || sizeof(A) % (count * sizeof(S)) != 0)
            {
                throw std::invalid_argument("The verilated port is not an array of " + std::to_string(count) + " packed structs");
            }
            const std::size_t words = sizeof(A) / sizeof(S) / count;
            std::vector<S *> pointers(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                pointers[i] = vl_elements<S>(ext_port) + i * words;
            }
            return pointers;
        }


        /// How the port structs of an array of verilated structs are made (PortStructArrayOf<S>::Factory): the
        /// struct i of class S is constructed from its name and the verilated struct *exts[i], or, for packed
        /// structs, from the pointer exts[i] to the first word of its vector.
        template <typename S, typename E>
        auto vl_structs(std::vector<E *> exts)
        {
            return [exts = std::move(exts)](const std::string &name, std::size_t i) -> std::unique_ptr<S>
            {
                if constexpr (std::is_class_v<E>)
                {
                    return std::make_unique<S>(name, *exts.at(i));
                }
                else
                {
                    return std::make_unique<S>(name, exts.at(i));
                }
            };
        }
    } // namespace detail

    /// Array of input ports bound to a verilated unpacked array port. Elements map to the array in row-major order.
    template <typename T, int W>
    class VInputArray final : public InputArray<T>
    {
    public:
        using Base = InputArray<T>;
        using Storage = detail::vl_storage_t<T>;

        /// ext_port is the port of the verilated model (VlUnpacked, nested for more dimensions).
        template <detail::VlArrayLike A>
        VInputArray(const std::string &name, A &ext_port)
            : InputArray<T>(name, detail::vl_shape_of<A>(), [&](const std::string &n, std::size_t i)
                            { return std::make_unique<VInput<T, W>>(n, detail::vl_elements<Storage>(ext_port)[i]); })
        {
        }
    };

    /// Array of output ports bound to a verilated unpacked array port. Elements map to the array in row-major order.
    template <typename T, int W>
    class VOutputArray final : public OutputArray<T>
    {
    public:
        using Base = OutputArray<T>;
        using Storage = detail::vl_storage_t<T>;

        /// ext_port is the port of the verilated model (VlUnpacked, nested for more dimensions).
        template <detail::VlArrayLike A>
        VOutputArray(const std::string &name, A &ext_port)
            : OutputArray<T>(name, detail::vl_shape_of<A>(), [&](const std::string &n, std::size_t i)
                             { return std::make_unique<VOutput<T, W>>(n, detail::vl_elements<Storage>(ext_port)[i]); })
        {
        }
    };
    namespace detail
    {
        /// Bits in one word of a verilated port wider than 64 bits (Verilator's EData).
        inline constexpr int vl_word_bits = 32;

        /// Number of words that hold a W-bit port.
        template <int W>
        inline constexpr std::size_t vl_words = (W + vl_word_bits - 1) / vl_word_bits;

        /// Width of the most significant word of a W-bit port.
        template <int W>
        inline constexpr int vl_top_word_bits = W - static_cast<int>(vl_words<W> - 1) * vl_word_bits;

        /// Shape of the dspsim port for a W-bit wide verilated port A: the shape of A with its words as the last
        /// dimension. Throws std::invalid_argument if A does not hold W bits per element.
        template <int W, typename A>
        Shape vl_wide_shape(const std::string &name)
        {
            static_assert(W > 64, "A wide port has more than 64 bits. Narrower ports are VInput/VOutput.");
            Shape shape = vl_shape_of<A>();
            if (shape.empty() || shape.back() != vl_words<W>)
            {
                throw std::invalid_argument("Port " + name + " is not " + std::to_string(W) + " bits wide in the verilated model");
            }
            return shape;
        }
    } // namespace detail

    /// Input bound to a verilated port of more than 64 bits, or to an unpacked array of them. Verilator stores
    /// such a port as 32-bit words, least significant first (VlWide), and dspsim presents it the same way: an
    /// array of 32-bit inputs whose last dimension is the words. W is the HDL port width. The most significant
    /// word has the remaining W - 32 * (words - 1) bits and is truncated to them when written to the model.
    template <int W>
    class VInputWide final : public InputArray<uint32_t>
    {
    public:
        using Base = InputArray<uint32_t>;
        using Storage = uint32_t;

        /// ext_port is the port of the verilated model (VlWide, or VlUnpacked of VlWide).
        template <typename A>
        VInputWide(const std::string &name, A &ext_port)
            : InputArray<uint32_t>(name, detail::vl_wide_shape<W, A>(name), [&](const std::string &n, std::size_t i) -> std::unique_ptr<Input<uint32_t>>
                                   {
                                       auto &word = detail::vl_elements<Storage>(ext_port)[i];
                                       if (i % detail::vl_words<W> == detail::vl_words<W> - 1)
                                       {
                                           return std::make_unique<VInput<uint32_t, detail::vl_top_word_bits<W>>>(n, word);
                                       }
                                       return std::make_unique<VInput<uint32_t, detail::vl_word_bits>>(n, word);
                                   })
        {
        }
    };

    /// Output bound to a verilated port of more than 64 bits, or to an unpacked array of them: an array of 32-bit
    /// outputs whose last dimension is the words, least significant first. See VInputWide.
    template <int W>
    class VOutputWide final : public OutputArray<uint32_t>
    {
    public:
        using Base = OutputArray<uint32_t>;
        using Storage = uint32_t;

        /// ext_port is the port of the verilated model (VlWide, or VlUnpacked of VlWide).
        template <typename A>
        VOutputWide(const std::string &name, A &ext_port)
            : OutputArray<uint32_t>(name, detail::vl_wide_shape<W, A>(name), [&](const std::string &n, std::size_t i) -> std::unique_ptr<Output<uint32_t>>
                                    {
                                        auto &word = detail::vl_elements<Storage>(ext_port)[i];
                                        if (i % detail::vl_words<W> == detail::vl_words<W> - 1)
                                        {
                                            return std::make_unique<VOutput<uint32_t, detail::vl_top_word_bits<W>>>(n, word);
                                        }
                                        return std::make_unique<VOutput<uint32_t, detail::vl_word_bits>>(n, word);
                                    })
        {
        }
    };

    namespace detail
    {
        /// The words of a verilated packed port: its one integer up to 64 bits, 32-bit words above. Throws
        /// std::invalid_argument if bits [lsb + width - 1 : lsb] are not inside the port.
        template <typename S, typename A>
        S *vl_bits_of(const std::string &name, A &ext_port, int lsb, int width)
        {
            if (lsb < 0 || static_cast<std::size_t>(lsb) + width > sizeof(A) * CHAR_BIT)
            {
                throw std::invalid_argument("Field " + name + " is outside of the verilated port");
            }
            return vl_elements<S>(ext_port);
        }

        /// Read W bits (at most 64), starting at bit lsb, of an array of words.
        template <int W, typename S>
        constexpr uint64_t vl_get_bits(const S *words, int lsb)
        {
            constexpr int word_bits = sizeof(S) * CHAR_BIT;
            uint64_t value = 0;
            int done = 0;
            for (int word = lsb / word_bits, shift = lsb % word_bits; done < W; ++word, shift = 0)
            {
                value |= (static_cast<uint64_t>(words[word]) >> shift) << done;
                done += word_bits - shift;
            }
            return value & mask(W);
        }

        /// Write the low W bits (at most 64) of value to an array of words, starting at bit lsb. The other bits
        /// of the words are kept.
        template <int W, typename S>
        constexpr void vl_set_bits(S *words, int lsb, uint64_t value)
        {
            constexpr int word_bits = sizeof(S) * CHAR_BIT;
            int done = 0;
            for (int word = lsb / word_bits, shift = lsb % word_bits; done < W; ++word, shift = 0)
            {
                const int count = (word_bits - shift < W - done) ? word_bits - shift : W - done;
                const uint64_t field = mask(count) << shift;
                const uint64_t bits = ((value >> done) << shift) & field;
                words[word] = static_cast<S>((static_cast<uint64_t>(words[word]) & ~field) | bits);
                done += count;
            }
        }
    } // namespace detail

    /// Input bound to a field of a verilated packed struct port: the W bits that start at bit lsb of the port.
    /// S is the type of the words that Verilator stores the port in: the port's one integer up to 64 bits,
    /// uint32_t above. The other fields of the port are untouched when this one is written to the model.
    template <typename T, int W, typename S>
    class VInputBits final : public Input<T>
    {
        static_assert(std::is_integral_v<T>, "The fields of a packed struct are integral.");
        static_assert(detail::vl_port_check<T, W>());

    public:
        using Base = Input<T>;

        /// ext_port is the struct port of the verilated model.
        template <typename A>
        VInputBits(const std::string &name, A &ext_port, int lsb)
            : Input<T>(name, W), words_(detail::vl_bits_of<S>(name, ext_port, lsb, W)), lsb_(lsb)
        {
        }

        /// words points at the first word of the packed vector that the field is a part of.
        VInputBits(const std::string &name, S *words, int lsb)
            : Input<T>(name, W), words_(words), lsb_(lsb)
        {
        }

    private:
        /// Write the input value to its bits of the top model port.
        void sync() override
        {
            detail::vl_set_bits<W>(words_, lsb_, detail::vl_truncate<T, W>(this->read()));
        }

    private:
        S *words_;
        int lsb_;
    };

    /// Output bound to a field of a verilated packed struct port. Signed types are sign extended from W bits.
    template <typename T, int W, typename S>
    class VOutputBits final : public Output<T>
    {
        static_assert(std::is_integral_v<T>, "The fields of a packed struct are integral.");
        static_assert(detail::vl_port_check<T, W>());

    public:
        using Base = Output<T>;

        /// ext_port is the struct port of the verilated model.
        template <typename A>
        VOutputBits(const std::string &name, A &ext_port, int lsb)
            : Output<T>(name, W), words_(detail::vl_bits_of<S>(name, ext_port, lsb, W)), lsb_(lsb)
        {
        }

        /// words points at the first word of the packed vector that the field is a part of.
        VOutputBits(const std::string &name, S *words, int lsb)
            : Output<T>(name, W), words_(words), lsb_(lsb)
        {
        }

    private:
        /// Write its bits of the top model port to the output port.
        void sync() override
        {
            this->write(detail::vl_extend<T, W>(static_cast<detail::vl_storage_t<T>>(detail::vl_get_bits<W>(words_, lsb_))));
        }

    private:
        S *words_;
        int lsb_;
    };

    /// Input bound to a field of more than 64 bits of a verilated packed struct port: an array of 32-bit words,
    /// least significant first, like VInputWide. The field can start at any bit of the port.
    template <int W>
    class VInputWideBits final : public InputArray<uint32_t>
    {
        static_assert(W > 64, "A wide field has more than 64 bits. Narrower fields are VInputBits.");

    public:
        using Base = InputArray<uint32_t>;

        /// ext_port is the struct port of the verilated model.
        template <typename A>
        VInputWideBits(const std::string &name, A &ext_port, int lsb)
            : InputArray<uint32_t>(name, {detail::vl_words<W>}, [&](const std::string &n, std::size_t i) -> std::unique_ptr<Input<uint32_t>>
                                   {
                                       const int word_lsb = lsb + static_cast<int>(i) * detail::vl_word_bits;
                                       if (i == detail::vl_words<W> - 1)
                                       {
                                           return std::make_unique<VInputBits<uint32_t, detail::vl_top_word_bits<W>, uint32_t>>(n, ext_port, word_lsb);
                                       }
                                       return std::make_unique<VInputBits<uint32_t, detail::vl_word_bits, uint32_t>>(n, ext_port, word_lsb);
                                   })
        {
        }
    };

    /// Output bound to a field of more than 64 bits of a verilated packed struct port. See VInputWideBits.
    template <int W>
    class VOutputWideBits final : public OutputArray<uint32_t>
    {
        static_assert(W > 64, "A wide field has more than 64 bits. Narrower fields are VOutputBits.");

    public:
        using Base = OutputArray<uint32_t>;

        /// ext_port is the struct port of the verilated model.
        template <typename A>
        VOutputWideBits(const std::string &name, A &ext_port, int lsb)
            : OutputArray<uint32_t>(name, {detail::vl_words<W>}, [&](const std::string &n, std::size_t i) -> std::unique_ptr<Output<uint32_t>>
                                    {
                                        const int word_lsb = lsb + static_cast<int>(i) * detail::vl_word_bits;
                                        if (i == detail::vl_words<W> - 1)
                                        {
                                            return std::make_unique<VOutputBits<uint32_t, detail::vl_top_word_bits<W>, uint32_t>>(n, ext_port, word_lsb);
                                        }
                                        return std::make_unique<VOutputBits<uint32_t, detail::vl_word_bits, uint32_t>>(n, ext_port, word_lsb);
                                    })
        {
        }
    };

    /// Access a verilated port through its registered dspsim base class (Input<T>, OutputArray<T>, ...).
    /// Each width produces a distinct VInput/VOutput type, so bindings expose ports by their base type.
    template <typename P>
    typename P::Base &vport_base(P &port)
    {
        return port;
    }
} // namespace dspsim

// Implementation macros. T is the dspsim-side element type.
#define DSPSIM_VPORT_(cls, T, name, width) \
    ::dspsim::cls<T, width> name { #name, top->name }

#define DSPSIM_VPORT_ARRAY_(cls, T, name, width) \
    ::dspsim::cls<T, width> name { #name, top->name }

// Declare an unsigned (or real) input on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VINPUT(name, width) \
    DSPSIM_VPORT_(VInput, ::dspsim::detail::vl_element_t<decltype(top->name)>, name, width)

// Declare an unsigned (or real) output on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VOUTPUT(name, width) \
    DSPSIM_VPORT_(VOutput, ::dspsim::detail::vl_element_t<decltype(top->name)>, name, width)

// Declare a signed input on a vmodule. Values are truncated to width bits when written to the model.
#define DSPSIM_VINPUT_S(name, width) \
    DSPSIM_VPORT_(VInput, ::dspsim::detail::vl_signed_t<decltype(top->name)>, name, width)

// Declare a signed output on a vmodule. Values are sign extended from width bits.
#define DSPSIM_VOUTPUT_S(name, width) \
    DSPSIM_VPORT_(VOutput, ::dspsim::detail::vl_signed_t<decltype(top->name)>, name, width)

// Declare an input array on a vmodule. The shape and element type are deduced from the verilated port.
#define DSPSIM_VINPUT_ARRAY(name, width) \
    DSPSIM_VPORT_ARRAY_(VInputArray, ::dspsim::detail::vl_element_t<decltype(top->name)>, name, width)

// Declare an output array on a vmodule. The shape and element type are deduced from the verilated port.
#define DSPSIM_VOUTPUT_ARRAY(name, width) \
    DSPSIM_VPORT_ARRAY_(VOutputArray, ::dspsim::detail::vl_element_t<decltype(top->name)>, name, width)

// Declare a signed input array on a vmodule.
#define DSPSIM_VINPUT_ARRAY_S(name, width) \
    DSPSIM_VPORT_ARRAY_(VInputArray, ::dspsim::detail::vl_signed_t<decltype(top->name)>, name, width)

// Declare a signed output array on a vmodule.
#define DSPSIM_VOUTPUT_ARRAY_S(name, width) \
    DSPSIM_VPORT_ARRAY_(VOutputArray, ::dspsim::detail::vl_signed_t<decltype(top->name)>, name, width)

// Declare an input wider than 64 bits (or an unpacked array of them) on a vmodule: an array of 32-bit words,
// least significant first, as the last dimension.
#define DSPSIM_VINPUT_WIDE(name, width) \
    ::dspsim::VInputWide<width> name { #name, top->name }

// Declare an output wider than 64 bits (or an unpacked array of them) on a vmodule.
#define DSPSIM_VOUTPUT_WIDE(name, width) \
    ::dspsim::VOutputWide<width> name { #name, top->name }
