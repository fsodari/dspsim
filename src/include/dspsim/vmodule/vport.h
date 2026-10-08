#pragma once
#include <dspsim/port.h>
#include <type_traits>

namespace dspsim
{
    // Ports used for VModels. Can bind to the verilator model ports.
    template <typename T>
    class VInput final : public Input<T>
    {

    public:
        VInput(const std::string &name, int width, T &ext_port) : Input<T>(name, width)
        {
            ext_port_ = &ext_port;
        }

        // write the input value to the top model port.
        void sync() override { *ext_port_ = this->read(); }

    private:
        T *ext_port_;
    };

    template <typename T>
    class VOutput final : public Output<T>
    {

    public:
        // Initialize the port with the verilated model's port.
        VOutput(const std::string &name, int width, T &ext_port) : Output<T>(name, width)
        {
            ext_port_ = &ext_port;
        }

        // Write the top model value to the output port.
        void sync() override { this->write(*ext_port_); }

    private:
        T *ext_port_;
    };


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
            vl_shape<A>(shape);
            return shape;
        }
    } // namespace detail

    // Array of input ports bound to a verilated unpacked array port. Elements map to the array in row-major order.
    template <typename T>
    class VInputArray final : public InputArray<T>
    {
    public:
        VInputArray(const std::string &name, Shape shape, int width, T *ext_port)
            : InputArray<T>(name, std::move(shape), [&](const std::string &n, std::size_t i)
                            { return std::make_unique<VInput<T>>(n, width, ext_port[i]); })
        {
        }
    };

    template <typename T>
    class VOutputArray final : public OutputArray<T>
    {
    public:
        VOutputArray(const std::string &name, Shape shape, int width, T *ext_port)
            : OutputArray<T>(name, std::move(shape), [&](const std::string &n, std::size_t i)
                             { return std::make_unique<VOutput<T>>(n, width, ext_port[i]); })
        {
        }
    };
}

// Declare an input on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VINPUT(name, width) \
    ::dspsim::VInput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }

// Declare an output on a vmodule. Automatically binds to the top module's port.
#define DSPSIM_VOUTPUT(name, width) \
    ::dspsim::VOutput<std::remove_reference_t<decltype(top->name)>> name { #name, width, top->name }

// Declare an input array on a vmodule. The shape and element type are deduced from the verilated port.
#define DSPSIM_VINPUT_ARRAY(name, width)                                                                  \
    ::dspsim::VInputArray<typename ::dspsim::detail::vl_element<std::remove_reference_t<decltype(top->name)>>::type> name \
    {                                                                                                     \
        #name, ::dspsim::detail::vl_shape_of<std::remove_reference_t<decltype(top->name)>>(), width,      \
            reinterpret_cast<typename ::dspsim::detail::vl_element<std::remove_reference_t<decltype(top->name)>>::type *>(&top->name) \
    }

// Declare an output array on a vmodule. The shape and element type are deduced from the verilated port.
#define DSPSIM_VOUTPUT_ARRAY(name, width)                                                                 \
    ::dspsim::VOutputArray<typename ::dspsim::detail::vl_element<std::remove_reference_t<decltype(top->name)>>::type> name \
    {                                                                                                     \
        #name, ::dspsim::detail::vl_shape_of<std::remove_reference_t<decltype(top->name)>>(), width,      \
            reinterpret_cast<typename ::dspsim::detail::vl_element<std::remove_reference_t<decltype(top->name)>>::type *>(&top->name) \
    }
