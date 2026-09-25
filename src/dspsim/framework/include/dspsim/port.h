#pragma once
#include <dspsim/signal.h>
#include <dspsim/event.h>
#include <memory>
#include <vector>

namespace dspsim
{
    class PortBase : public Model
    {
        int _width;

    public:
        PortBase(const std::string &name, int width, const std::string &kind);
        virtual void finalize() override = 0;
        // VPorts need to use this. How can I avoid this coupling? VPorts should use composition instead of inheritance?
        virtual void _sync() {}
    };

    class InputBase : public PortBase
    {
    protected:
        // During elaboration, these events will be added to the bound signal's events.
        SensitivityEvent _change_event;
        SensitivityEvent _posedge_event;
        SensitivityEvent _negedge_event;

    public:
        InputBase(const std::string &name, int width);
        virtual void finalize() override = 0;

        // Processes can be sensitive to port changes.
        SensitivityEvent *_change() { return &_change_event; }
        SensitivityEvent *pos() { return &_posedge_event; }
        SensitivityEvent *neg() { return &_negedge_event; }

        // Cast this class as _change() event when using in a sensitivity list.
        operator SensitivityEvent *() { return _change(); }
    };

    template <typename T>
    class Input : public InputBase
    {
        Signal<T> *_bound_signal = nullptr;
        std::vector<Input<T> *> _bound_ports;

    public:
        Input(const std::string &name, int width = default_bitwidth<T>::value);
        void finalize() override;

    protected:
        // Update the bound signal's subscribers with the ports subscribers
        void update_bound_signal_subscribers();
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

    public:
        void bind(Signal<T> &signal);
        void bind(Input<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal);
        void _bind_port(Input<T> &port);

        // Read the value of the port (bound signal).
        const T &read() const { return _bound_signal->read(); }

        // Set in the update cycle after a signal event. Derived from the bound signal.
        bool changed() const { return _bound_signal->changed(); }
        bool posedge() const { return _bound_signal->posedge(); }
        bool negedge() const { return _bound_signal->negedge(); }
    };

    class OutputBase : public PortBase
    {
    public:
        OutputBase(const std::string &name, int width);
        virtual void finalize() override = 0;
    };

    template <typename T>
    class Output : public OutputBase
    {
    private:
        Signal<T> *_bound_signal = nullptr;
        std::vector<Output<T> *> _bound_ports;

    public:
        Output(const std::string &name, int width = default_bitwidth<T>::value);
        void finalize() override;

    protected:
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

    public:
        void bind(Signal<T> &signal);
        void bind(Output<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal);
        void _bind_port(Output<T> &port);

        // Read the value of the bound signal.
        const T &read() const { return _bound_signal->read(); }

        // Write to the bound signal.
        void write(const T &value) { _bound_signal->write(value); }

        // Read the pending value. Shouldn't be used, but is available.
        const T &_read_d() const { return _bound_signal->_read_d(); }
    };

}
