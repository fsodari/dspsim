#pragma once
#include <dspsim/signal.h>
#include <dspsim/forward.h>
#include <dspsim/event.h>
#include <dspsim/sensitivity_list.h>
#include <memory>
#include <vector>

namespace dspsim
{
    class PortBase : public Model
    {
    public:
        PortBase(const std::string &name);
        virtual const std::string kind() const { return "port"; }
        virtual void _notify(EventType event) = 0;
        virtual void finalize() override;
    };

    class InputBase : public PortBase
    {
    public:
        InputBase(const std::string &name);
        virtual void finalize() override;
        virtual void _notify(EventType event) override;

        // When using this port in a sensitivity list, it can be cast to std::vector<Module *> to obtain the list of changed subscribers.
        SensitivityEvent &pos();
        SensitivityEvent &neg();
        SensitivityEvent &change();

        // Cast this class as std::vector<Module *> when using in a sensitivity list.
        operator SensitivityEvent &();

    protected:
        std::vector<Module *> _changed_subscribers;
        std::vector<Module *> _posedge_subscribers;
        std::vector<Module *> _negedge_subscribers;
    };

    template <typename T>
    class Input : public InputBase
    {
    public:
        Input(const std::string &name);
        Input(const std::string &name, Signal<T> &signal);
        operator Signal<T> &();
        void bind(Signal<T> &signal);
        void bind(Input<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal) { bind(signal); }
        void _bind_port(Input<T> &port) { bind(port); }
        void finalize() override;

        const T &read() const;

    protected:
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

        Signal<T> *_bound_tsignal = nullptr;
        std::vector<Input<T> *> _bound_ports;
    };

    // class OutputBase : public PortBase
    // {
    // public:
    //     OutputBase(const std::string &name) : PortBase(name) {}
    //     virtual void notify(EventType event) override {}
    // };

    template <typename T>
    class Output : public PortBase
    {
    public:
        Output(const std::string &name);
        Output(const std::string &name, Signal<T> &signal);
        operator Signal<T> &();
        void bind(Signal<T> &signal);
        void bind(Output<T> &port);

        // Explicit functions for python bindings
        void _bind_signal(Signal<T> &signal) { bind(signal); }
        void _bind_port(Output<T> &port) { bind(port); }
        void _notify(EventType event) override;
        void finalize() override;
        void write(const T &value);

        const T &_read_d() const { return _bound_tsignal->_read_d(); }
        const T &_read() const { return _bound_tsignal->read(); }

    protected:
        // Resolve a chain of port-to-port bindings down to the underlying signal.
        void resolve();

        Signal<T> *_bound_tsignal = nullptr;
        std::vector<Output<T> *> _bound_ports;
    };

}
