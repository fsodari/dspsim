#pragma once
#include <dspsim/signal.h>
#include <dspsim/event.h>
#include <dspsim/ndarray.h>
#include <memory>
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
