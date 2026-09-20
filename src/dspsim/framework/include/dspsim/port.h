#pragma once
#include <dspsim/signal.h>
// #include <dspsim/forward.h>
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
        virtual void _notify(EventType event) = 0;
    };

    class InputBase : public PortBase
    {
    protected:
        SensitivityEvent _change_event;
        SensitivityEvent _posedge_event;
        SensitivityEvent _negedge_event;

    public:
        InputBase(const std::string &name);
        virtual void finalize() override = 0;

        // Gets called when there is an event on this port.
        virtual void _notify(EventType event) override;

        // When using this port in a sensitivity list, it can be cast to std::vector<Module *> to obtain the list of changed subscribers.
        SensitivityEvent &pos();
        SensitivityEvent &neg();
        SensitivityEvent &_change();

        // Cast this class as _change() event when using in a sensitivity list.
        operator SensitivityEvent &();
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
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

    public:
        operator Signal<T> &();
        void bind(Signal<T> &signal);
        void bind(Input<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal);
        void _bind_port(Input<T> &port);

        const T &read() const;

        // Set in the update cycle after a signal event. Derived from bound signal.
        bool posedge() const;
        bool negedge() const;
        bool changed() const;
    };

    class OutputBase : public PortBase
    {
    public:
        OutputBase(const std::string &name) : PortBase(name, "output") {}
        virtual void finalize() override = 0;
        virtual void _notify(EventType event) override = 0;
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
        virtual void _notify(EventType event) override {}

        operator Signal<T> &();
        void bind(Signal<T> &signal);
        void bind(Output<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal);
        void _bind_port(Output<T> &port);

        void write(const T &value);

        const T &_read_d() const;
        const T &read() const;
    };

}
