#pragma once
#include <dspsim/signal.h>
#include <dspsim/forward.h>
#include <memory>
#include <vector>

namespace dspsim
{
    class PortBase : public Model
    {
    public:
        enum EventType
        {
            Changed,
            Posedge,
            Negedge
        };
        PortBase(const std::string &name);
        virtual const std::string kind() const { return "port"; }
        virtual void notify(EventType event) = 0;
        virtual void finalize() override;
    };

    class InputBase : public PortBase
    {
        friend class SensitivityList;

    public:
        InputBase(const std::string &name);
        virtual void finalize() override;
        virtual void notify(EventType event) override;

        std::vector<Module *> &pos();
        std::vector<Module *> &neg();

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
        virtual void bind(Signal<T> &signal);
        virtual void bind(Input<T> &port);

        const T &read() const;

    protected:
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
        virtual void notify(EventType event) override;
        void write(const T &value);

    protected:
        Signal<T> *_bound_tsignal = nullptr;
        std::vector<Output<T> *> _bound_ports;
    };

}
