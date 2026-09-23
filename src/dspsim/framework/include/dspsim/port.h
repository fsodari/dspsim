#pragma once
#include <dspsim/signal.h>
#include <dspsim/event.h>
#include <dspsim/sensitivity_list.h>
#include <memory>
#include <vector>

namespace dspsim
{
    class PortBase : public Model
    {
    public:
        PortBase(const std::string &name, const std::string &kind);
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
        InputBase(const std::string &name);
        virtual void finalize() override = 0;

        // Modules can be sensitive to port changes.
        // Defined inline: bind-time only, but trivial and cheap to keep consistent with signal.h.
        SensitivityEvent *pos() { return &_posedge_event; }
        SensitivityEvent *neg() { return &_negedge_event; }
        SensitivityEvent *_change() { return &_change_event; }

        // Cast this class as _change() event when using in a sensitivity list.
        operator SensitivityEvent *() { return _change(); }
    };

    template <typename T>
    class Input : public InputBase
    {
        Signal<T> *_bound_signal = nullptr;
        std::vector<Input<T> *> _bound_ports;

    public:
        Input(const std::string &name);
        Input(const std::string &name, Signal<T> &signal);
        void finalize() override;

    protected:
        // Update the bound signal's subscribers with the ports subscribers
        void update_bound_signal_subscribers();
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

    public:
        operator Signal<T> &();
        void bind(Signal<T> &signal);
        void bind(Input<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal);
        void _bind_port(Input<T> &port);

        // Defined inline: called every eval() in the hot path, must be inlinable without LTO.
        const T &read() const { return _bound_signal->read(); }

        // Set in the update cycle after a signal event. Derived from bound signal.
        bool posedge() const { return _bound_signal->posedge(); }
        bool negedge() const { return _bound_signal->negedge(); }
        bool changed() const { return _bound_signal->changed(); }
    };

    class OutputBase : public PortBase
    {
    public:
        OutputBase(const std::string &name) : PortBase(name, "output") {}
        virtual void finalize() override = 0;
    };

    template <typename T>
    class Output : public OutputBase
    {
    private:
        Signal<T> *_bound_signal = nullptr;
        std::vector<Output<T> *> _bound_ports;

    public:
        Output(const std::string &name);
        Output(const std::string &name, Signal<T> &signal);
        void finalize() override;

    protected:
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

    public:
        operator Signal<T> &();
        void bind(Signal<T> &signal);
        void bind(Output<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal);
        void _bind_port(Output<T> &port);

        // Defined inline: called every eval() in the hot path, must be inlinable without LTO.
        void write(const T &value)
        {
            _bound_signal->write(value);
            for (auto &port : _bound_ports)
            {
                port->write(value);
            }
        }

        const T &_read_d() const { return _bound_signal->_read_d(); }
        const T &read() const { return _bound_signal->read(); }
    };

}
