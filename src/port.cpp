#include <dspsim/port.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    PortBase::PortBase(const std::string &name, const std::string &kind)
        : Model(name, kind)
    {
        if (context()->_active_module())
        {
            context()->_active_module()->_ports.push_back(this);
        }
        else
        {
            context()->logger->error("No active module to register port {}", name);
        }
    }

    InputBase::InputBase(const std::string &name) : PortBase(name, "input")
    {
        // Register an input port with the parent module.
        if (context()->_active_module())
        {
            context()->_active_module()->_inputs.push_back(this);
        }
        else
        {
            context()->logger->error("No active module to register input port {}", name);
        }
    }

    template <typename T>
    Input<T>::Input(const std::string &name) : InputBase(name)
    {
    }

    template <typename T>
    Input<T>::Input(const std::string &name, Signal<T> &signal) : InputBase(name)
    {
        bind(signal);
    }
    template <typename T>
    void Input<T>::finalize()
    {
        resolve();
    }

    template <typename T>
    void Input<T>::update_bound_signal_subscribers()
    {
        _bound_signal->pos()->processes().push_range(_posedge_event.processes());
        _bound_signal->neg()->processes().push_range(_negedge_event.processes());
        _bound_signal->_change()->processes().push_range(_change_event.processes());
    }

    template <typename T>
    void Input<T>::resolve()
    {
        if (_bound_signal)
        {
            update_bound_signal_subscribers();
            // What if downstream ports need to be bound? Can this happen in an input?

            return;
        }
        for (auto *port : _bound_ports)
        {
            port->resolve();
            if (port->_bound_signal)
            {
                _bound_signal = port->_bound_signal;
                // Add the ports events to the bound signal's events.
                update_bound_signal_subscribers();
                break;
            }
        }
        if (!_bound_signal)
        {
            context()->logger->error("Input port {} could not be resolved to a signal", hier_name());
        }
    }

    template <typename T>
    Input<T>::operator Signal<T> &()
    {
        return *_bound_signal;
    }

    template <typename T>
    void Input<T>::bind(Signal<T> &signal)
    {
        if (_bound_signal)
        {
            context()->logger->error("Input port {} is already bound to a signal", name());
        }
        _bound_signal = &signal;
    }

    template <typename T>
    void Input<T>::bind(Input<T> &port)
    {
        _bound_ports.push_back(&port);
    }
    template <typename T>
    void Input<T>::_bind_signal(Signal<T> &signal)
    {
        bind(signal);
    }
    template <typename T>
    void Input<T>::_bind_port(Input<T> &port)
    {
        bind(port);
    }

    //
    // OUTPUT<T>
    //

    OutputBase::OutputBase(const std::string &name) : PortBase(name, "output")
    {
        if (context()->_active_module())
        {
            context()->_active_module()->_outputs.push_back(this);
        }
        else
        {
            context()->logger->error("No active module to register output port {}", name);
        }
    }

    template <typename T>
    Output<T>::Output(const std::string &name) : OutputBase(name)
    {
    }

    template <typename T>
    Output<T>::Output(const std::string &name, Signal<T> &signal) : Output<T>(name)
    {
        bind(signal);
    }

    template <typename T>
    void Output<T>::finalize()
    {
        resolve();
    }

    template <typename T>
    void Output<T>::resolve()
    {
        if (_bound_signal)
        {
            // What if downstream ports need to be bound? Can this happen in an output?
            return;
        }
        for (auto *port : _bound_ports)
        {
            port->resolve();
            if (port->_bound_signal)
            {
                _bound_signal = port->_bound_signal;
                break;
            }
        }
        if (!_bound_signal)
        {
            context()->logger->error("Output port {} could not be resolved to a signal", hier_name());
        }
    }

    template <typename T>
    Output<T>::operator Signal<T> &()
    {
        return *_bound_signal;
    }

    template <typename T>
    void Output<T>::bind(Signal<T> &signal)
    {
        if (_bound_signal)
        {
            context()->logger->error("Output port {} is already bound to a signal", hier_name());
        }
        _bound_signal = &signal;
    }

    template <typename T>
    void Output<T>::bind(Output<T> &port)
    {
        _bound_ports.push_back(&port);
    }

    template <typename T>
    void Output<T>::_bind_signal(Signal<T> &signal)
    {
        bind(signal);
    }

    template <typename T>
    void Output<T>::_bind_port(Output<T> &port)
    {
        bind(port);
    }

    template class Input<uint8_t>;
    template class Input<uint16_t>;
    template class Input<uint32_t>;
    template class Input<uint64_t>;
    template class Input<int>;
    template class Input<float>;
    template class Input<double>;

    template class Output<uint8_t>;
    template class Output<uint16_t>;
    template class Output<uint32_t>;
    template class Output<uint64_t>;
    template class Output<int>;
    template class Output<float>;
    template class Output<double>;
}
