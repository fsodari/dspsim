#pragma once
#include <dspsim/port.h>
#include <dspsim/bits.h>
#include <climits>
#include <type_traits>

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
    } // namespace detail

    /// Array of input ports bound to a verilated unpacked array port. Elements map to the array in row-major order.
    template <typename T, int W>
    class VInputArray final : public InputArray<T>
    {
    public:
        using Base = InputArray<T>;
        using Storage = detail::vl_storage_t<T>;

        VInputArray(const std::string &name, Shape shape, Storage *ext_port)
            : InputArray<T>(name, std::move(shape), [&](const std::string &n, std::size_t i)
                            { return std::make_unique<VInput<T, W>>(n, ext_port[i]); })
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

        VOutputArray(const std::string &name, Shape shape, Storage *ext_port)
            : OutputArray<T>(name, std::move(shape), [&](const std::string &n, std::size_t i)
                             { return std::make_unique<VOutput<T, W>>(n, ext_port[i]); })
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

#define DSPSIM_VPORT_ARRAY_(cls, T, name, width)                                                   \
    ::dspsim::cls<T, width> name                                                                   \
    {                                                                                              \
        #name, ::dspsim::detail::vl_shape_of<decltype(top->name)>(),                               \
            reinterpret_cast<::dspsim::detail::vl_element_t<decltype(top->name)> *>(&top->name)    \
    }

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
