#include <dspsim/port.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    PortBase::PortBase(const std::string &name, const std::string &kind)
        : Model(name, kind)
    {
    }

    InputBase::InputBase(const std::string &name) : PortBase(name, "input")
    {
    }

    SensitivityEvent &InputBase::pos()
    {
        return _posedge_event;
    }
    SensitivityEvent &InputBase::neg()
    {
        return _negedge_event;
    }
    SensitivityEvent &InputBase::_change()
    {
        return _change_event;
    }
    InputBase::operator SensitivityEvent &()
    {
        return _change();
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
    void Input<T>::resolve()
    {
        if (_bound_signal)
        {
            // Add the ports events to the bound signal's events.
            _bound_signal->pos().subscribers().push_range(_posedge_event.subscribers());
            _bound_signal->neg().subscribers().push_range(_negedge_event.subscribers());
            _bound_signal->_change().subscribers().push_range(_change_event.subscribers());
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
                _bound_signal->pos().subscribers().push_range(_posedge_event.subscribers());
                _bound_signal->neg().subscribers().push_range(_negedge_event.subscribers());
                _bound_signal->_change().subscribers().push_range(_change_event.subscribers());
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

    template <typename T>
    const T &Input<T>::read() const
    {
        return _bound_signal->read();
    }

    // Set in the update cycle after a signal event. Derived from bound signal.
    template <typename T>
    bool Input<T>::posedge() const
    {
        return _bound_signal->posedge();
    }

    template <typename T>
    bool Input<T>::negedge() const
    {
        return _bound_signal->negedge();
    }

    template <typename T>
    bool Input<T>::changed() const
    {
        return _bound_signal->changed();
    }

    //
    // OUTPUT<T>
    //

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

    template <typename T>
    void Output<T>::write(const T &value)
    {
        _bound_signal->write(value);
        for (auto &port : _bound_ports)
        {
            port->write(value);
        }
    }

    template <typename T>
    const T &Output<T>::_read_d() const
    {
        return _bound_signal->_read_d();
    }

    template <typename T>
    const T &Output<T>::read() const
    {
        return _bound_signal->read();
    }

    template class Input<uint8_t>;
    template class Input<uint16_t>;
    template class Input<uint32_t>;
    template class Input<uint64_t>;
    template class Input<int>;

    template class Output<uint8_t>;
    template class Output<uint16_t>;
    template class Output<uint32_t>;
    template class Output<uint64_t>;
    template class Output<int>;
}
