#pragma once
#include <dspsim/signal.h>
#include <dspsim/event.h>
#include <dspsim/ndarray.h>
#include <memory>
#include <type_traits>
#include <vector>

namespace dspsim
{
    /*
        Ports represent the interface between modules and signals in the simulation framework.
        Ports can be instantiated within a module and bound to signals
        or parent ports to facilitate communication between different parts of the simulation.
    */
    class PortBase : public Model
    {
    public:
        // Constructor for the port base class. Initializes the port with a name, width, and kind.
        // kind will be input/output.
        PortBase(const std::string &name, int width, const std::string &kind);
        virtual ~PortBase() = default;

        // Get the width of the port.
        int width() const { return width_; }

        // Processes can be sensitive to port changes.
        SensitivityEvent &change() { return *change_event_; }
        SensitivityEvent &pos() { return *posedge_event_; }
        SensitivityEvent &neg() { return *negedge_event_; }

        // Implicit conversion to the change event. This may interfere with implicit conversions to the port value...
        operator SensitivityEvent &() { return *change_event_; }

        /*
            Called automatically during elaboration.
            This is where the port finalizes its binding to signals and updates its sensitivity events.
            Should this be private and the context a friend?
        */
        void finalize() override;

        // VPorts need to use this. How can I avoid this coupling? VPorts should use composition instead of inheritance?
        virtual void sync() {}

    protected:
        // Provides access to the bound signal.
        SignalBase *bound_signal() const { return bound_signal_; }

        // Bind functions. These will check for width and hierarchical level constraints.
        // Input/Output classes enforce binding to the correct signal type with their public interface.
        void bind_base(SignalBase &signal);
        void bind_base(PortBase &port);

    private:
        // Recursively search bound ports to resolve the final bound signal.
        void resolve();
        // Update the bound signal's subscribers with the ports subscribers
        void update_bound_signal_subscribers();

    protected:
        // Ports must be bound to a signal before the simulation starts.
        SignalBase *bound_signal_ = nullptr;

    private:
        // Width of the port.
        int width_;

        /*
            During construction, processes can specify static sensitivity to signals.
            Since the ports will not be bound to a signal at this stage, ports need to expose an event interface.
        */
        SensitivityEvent static_change_event_;
        SensitivityEvent static_posedge_event_;
        SensitivityEvent static_negedge_event_;

        // After elaboration, the port's sensitivity event will be associated with the bound signal's events.
        SensitivityEvent *change_event_;
        SensitivityEvent *posedge_event_;
        SensitivityEvent *negedge_event_;

        /*
            Submodule ports can bind to the ports of parent modules.
            Ports may not be bound between modules at the same hierarchical level.
        */
        std::vector<PortBase *> bound_ports_;
    };

    template <typename T>
    class Input : public PortBase
    {
    public:
        Input(const std::string &name, int width = default_bitwidth<T>::value);

        // Bind the input port to a signal.
        void bind(Signal<T> &signal) { bind_base(signal); }
        // Bind the input port to another input port.
        void bind(Input<T> &port) { bind_base(port); }
        /// Bind the input port to a bit selection (a slice or pack of signals) of the same width.
        /// The selected bits are zero or sign extended to T. Must be called before elaboration.
        void bind(const BitSel &selection)
            requires std::is_integral_v<T>;

        // Read the value of the port (bound signal).
        const T &read() const { return static_cast<Signal<T> *>(bound_signal_)->read(); }

        // Implicit conversion to read the value of the bound signal from the port
        operator const T &() const { return read(); }

        // Explicit functions for nanobind bindings
        void bind_signal(Signal<T> &signal) { bind_base(signal); }
        void bind_port(Input<T> &port) { bind_base(port); }
    };

    template <typename T>
    class Output : public PortBase
    {
    public:
        Output(const std::string &name, int width = default_bitwidth<T>::value);

        // Bind the output port to a signal.
        void bind(Signal<T> &signal) { bind_base(signal); }
        // Bind the output port to another output port.
        void bind(Output<T> &port) { bind_base(port); }
        /// Bind the output port to a bit selection (a slice or pack of signals) of the same width.
        /// Writes are truncated to the selection's width and passed through to the selected bits of the
        /// source signals at write time, so they combine with other writes to those signals in the same cycle
        /// (last write wins per bit). Must be called before elaboration.
        void bind(const BitSel &selection)
            requires std::is_integral_v<T>;

        // Read the value of the bound signal.
        const T &read() const { return static_cast<Signal<T> *>(bound_signal_)->read(); }
        // Implicit conversion to read the value of the bound signal from the port
        operator const T &() const { return read(); }

        // Write to the bound signal.
        void write(const T &value) { static_cast<Signal<T> *>(bound_signal_)->write(value); }
        // Implicit conversion to write to the bound signal from the port
        Output<T> &operator=(const T &value)
        {
            write(value);
            return *this;
        }

        // Explicit functions for python bindings
        void bind_signal(Signal<T> &signal) { bind_base(signal); }
        void bind_port(Output<T> &port) { bind_base(port); }

        // Read the pending value. Shouldn't be used, but is available.
        const T &read_d_() const { return static_cast<Signal<T> *>(bound_signal_)->read_d_(); }
    };

    template <typename T>
    class InputArray;

    /*
        Non-owning view of a selection of an InputArray (or another view), from InputArray::slice.
        Binds elementwise to a SignalArray/SignalArrayView or InputArray/InputArrayView of identical shape.
        The source arrays must outlive the view.
    */
    template <typename T>
    class InputArrayView : public detail::NdView<Input<T>>
    {
    public:
        InputArrayView(detail::NdView<Input<T>> v) : detail::NdView<Input<T>>(std::move(v)) {}

        InputArrayView view() const { return *this; }
        InputArrayView slice(const Slices &slices) const { return detail::NdView<Input<T>>::slice(slices); }
        InputArrayView select(const std::vector<Range> &ranges) const { return detail::NdView<Input<T>>::select(ranges); }

        void bind(const SignalArrayView<T> &signals) { bind_signal(signals); }
        void bind(const SignalArray<T> &signals) { bind_signal(signals.view()); }
        void bind(const InputArrayView &ports) { bind_port(ports); }
        void bind(const InputArray<T> &ports) { bind_port(ports.view()); }

        void bind_signal(const SignalArrayView<T> &signals)
        {
            check_shape(signals.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_signal(signals.flat(i));
        }
        void bind_signal(const SignalArray<T> &signals) { bind_signal(signals.view()); }
        void bind_port(const InputArrayView &ports)
        {
            check_shape(ports.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_port(ports.flat(i));
        }
        void bind_port(const InputArray<T> &ports) { bind_port(ports.view()); }

    private:
        void check_shape(const Shape &other) const
        {
            if (other != this->shape())
                throw std::invalid_argument("InputArrayView: shape mismatch");
        }
    };

    /*
        Multidimensional array of input ports. The shape is given at construction (any number of dimensions).
            InputArray<int> p{"p", {2, 3}};
        Elements are named "p[a][b]" and accessed with p[{a, b}] or p.at({a, b}).
        Can only be bound to a SignalArray or another InputArray of identical shape.
    */
    template <typename T>
    class InputArray : public detail::NdArray<Input<T>>
    {
    public:
        InputArray(const std::string &name, Shape shape, int width = default_bitwidth<T>::value)
            : detail::NdArray<Input<T>>(name, std::move(shape), [&](const std::string &n, std::size_t)
                                      { return std::make_unique<Input<T>>(n, width); })
        {
        }

        // Bind elementwise to a signal array.
        void bind(SignalArray<T> &signals) { bind_signal(signals); }
        // Bind elementwise to another input port array.
        void bind(InputArray<T> &ports) { bind_port(ports); }
        // Bind elementwise to a slice of signals or ports. Shapes must match exactly.
        void bind(const SignalArrayView<T> &signals) { bind_signal(signals); }
        void bind(const InputArrayView<T> &ports) { bind_port(ports); }

        // Views of the whole array or a sub-selection. One Slice per leading dimension; the rest are kept whole.
        InputArrayView<T> view() const { return detail::NdArray<Input<T>>::view(); }
        InputArrayView<T> slice(const Slices &slices) const { return detail::NdArray<Input<T>>::slice(slices); }
        InputArrayView<T> select(const std::vector<Range> &ranges) const { return detail::NdArray<Input<T>>::select(ranges); }

        // Explicit functions for python bindings
        void bind_signal(SignalArray<T> &signals)
        {
            check_shape(signals.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_signal(signals.flat(i));
        }
        void bind_port(InputArray<T> &ports)
        {
            check_shape(ports.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_port(ports.flat(i));
        }
        void bind_signal(const SignalArrayView<T> &signals)
        {
            check_shape(signals.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_signal(signals.flat(i));
        }
        void bind_port(const InputArrayView<T> &ports)
        {
            check_shape(ports.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_port(ports.flat(i));
        }

    protected:
        // For subclasses that need to construct their own element types (e.g. VInput).
        InputArray(const std::string &name, Shape shape, const typename detail::NdArray<Input<T>>::Factory &make)
            : detail::NdArray<Input<T>>(name, std::move(shape), make)
        {
        }

    private:
        void check_shape(const Shape &other) const
        {
            if (other != this->shape())
                throw std::invalid_argument("InputArray: shape mismatch");
        }
    };

    template <typename T>
    class OutputArray;

    /*
        Non-owning view of a selection of an OutputArray (or another view), from OutputArray::slice.
        Binds elementwise to a SignalArray/SignalArrayView or OutputArray/OutputArrayView of identical shape.
        The source arrays must outlive the view.
    */
    template <typename T>
    class OutputArrayView : public detail::NdView<Output<T>>
    {
    public:
        OutputArrayView(detail::NdView<Output<T>> v) : detail::NdView<Output<T>>(std::move(v)) {}

        OutputArrayView view() const { return *this; }
        OutputArrayView slice(const Slices &slices) const { return detail::NdView<Output<T>>::slice(slices); }
        OutputArrayView select(const std::vector<Range> &ranges) const { return detail::NdView<Output<T>>::select(ranges); }

        void bind(const SignalArrayView<T> &signals) { bind_signal(signals); }
        void bind(const SignalArray<T> &signals) { bind_signal(signals.view()); }
        void bind(const OutputArrayView &ports) { bind_port(ports); }
        void bind(const OutputArray<T> &ports) { bind_port(ports.view()); }

        void bind_signal(const SignalArrayView<T> &signals)
        {
            check_shape(signals.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_signal(signals.flat(i));
        }
        void bind_signal(const SignalArray<T> &signals) { bind_signal(signals.view()); }
        void bind_port(const OutputArrayView &ports)
        {
            check_shape(ports.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_port(ports.flat(i));
        }
        void bind_port(const OutputArray<T> &ports) { bind_port(ports.view()); }

    private:
        void check_shape(const Shape &other) const
        {
            if (other != this->shape())
                throw std::invalid_argument("OutputArrayView: shape mismatch");
        }
    };

    /*
        Multidimensional array of output ports. The shape is given at construction (any number of dimensions).
            OutputArray<int> p{"p", {2, 3}};
        Elements are named "p[a][b]" and accessed with p[{a, b}] or p.at({a, b}).
        Can only be bound to a SignalArray or another OutputArray of identical shape.
    */
    template <typename T>
    class OutputArray : public detail::NdArray<Output<T>>
    {
    public:
        OutputArray(const std::string &name, Shape shape, int width = default_bitwidth<T>::value)
            : detail::NdArray<Output<T>>(name, std::move(shape), [&](const std::string &n, std::size_t)
                                      { return std::make_unique<Output<T>>(n, width); })
        {
        }

        // Bind elementwise to a signal array.
        void bind(SignalArray<T> &signals) { bind_signal(signals); }
        // Bind elementwise to another output port array.
        void bind(OutputArray<T> &ports) { bind_port(ports); }
        // Bind elementwise to a slice of signals or ports. Shapes must match exactly.
        void bind(const SignalArrayView<T> &signals) { bind_signal(signals); }
        void bind(const OutputArrayView<T> &ports) { bind_port(ports); }

        // Views of the whole array or a sub-selection. One Slice per leading dimension; the rest are kept whole.
        OutputArrayView<T> view() const { return detail::NdArray<Output<T>>::view(); }
        OutputArrayView<T> slice(const Slices &slices) const { return detail::NdArray<Output<T>>::slice(slices); }
        OutputArrayView<T> select(const std::vector<Range> &ranges) const { return detail::NdArray<Output<T>>::select(ranges); }

        // Explicit functions for python bindings
        void bind_signal(SignalArray<T> &signals)
        {
            check_shape(signals.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_signal(signals.flat(i));
        }
        void bind_port(OutputArray<T> &ports)
        {
            check_shape(ports.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_port(ports.flat(i));
        }
        void bind_signal(const SignalArrayView<T> &signals)
        {
            check_shape(signals.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_signal(signals.flat(i));
        }
        void bind_port(const OutputArrayView<T> &ports)
        {
            check_shape(ports.shape());
            for (std::size_t i = 0; i < this->size(); ++i)
                this->flat(i).bind_port(ports.flat(i));
        }

    protected:
        // For subclasses that need to construct their own element types (e.g. VInput).
        OutputArray(const std::string &name, Shape shape, const typename detail::NdArray<Output<T>>::Factory &make)
            : detail::NdArray<Output<T>>(name, std::move(shape), make)
        {
        }

    private:
        void check_shape(const Shape &other) const
        {
            if (other != this->shape())
                throw std::invalid_argument("OutputArray: shape mismatch");
        }
    };
}
