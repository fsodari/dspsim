#include <dspsim/port.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    PortBase::PortBase(const std::string &name)
        : Model(name)
    {
    }

    void PortBase::finalize()
    {
    }
    InputBase::InputBase(const std::string &name) : PortBase(name)
    {
    }

    SensitivityEvent &InputBase::pos()
    {
        return _posedge_subscribers;
    }
    SensitivityEvent &InputBase::neg()
    {
        return _negedge_subscribers;
    }
    InputBase::operator SensitivityEvent &()
    {
        return _changed_subscribers;
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

    void InputBase::finalize()
    {
    }

    void InputBase::notify(EventType event)
    {
        if (event == EventType::Posedge)
        {
            for (auto subscriber : _posedge_subscribers)
            {
                // Notify the subscriber
                SPDLOG_LOGGER_TRACE(context()->logger, "Port {} notifying posedge subscriber: {}", name(), subscriber->name());
                context()->_push_eval_stack(subscriber);
            }
        }
        else if (event == EventType::Negedge)
        {
            for (auto subscriber : _negedge_subscribers)
            {
                // Notify the subscriber
                SPDLOG_LOGGER_TRACE(context()->logger, "Port {} notifying negedge subscriber: {}", name(), subscriber->name());
                context()->_push_eval_stack(subscriber);
            }
        }

        // We should only notify a subscriber once. The eval queue is a set so adding again is fine.
        for (auto subscriber : _changed_subscribers)
        {
            // Notify the subscriber
            SPDLOG_LOGGER_DEBUG(context()->logger, "Port {} notifying changed subscriber: {}", name(), subscriber->name());
            context()->_push_eval_stack(subscriber);
        }
    }

    template <typename T>
    Input<T>::operator Signal<T> &()
    {
        return *_bound_tsignal;
    }

    template <typename T>
    void Input<T>::bind(Signal<T> &signal)
    {
        if (_bound_tsignal)
        {
            context()->logger->error("Input port {} is already bound to a signal", name());
        }
        _bound_tsignal = &signal;
        _bound_tsignal->add_subscriber(this);
    }

    template <typename T>
    void Input<T>::bind(Input<T> &port)
    {
        _bound_ports.push_back(&port);
    }

    template <typename T>
    void Input<T>::finalize()
    {
        resolve();
    }

    template <typename T>
    void Input<T>::resolve()
    {
        if (_bound_tsignal)
        {
            return;
        }
        for (auto *port : _bound_ports)
        {
            port->resolve();
            if (port->_bound_tsignal)
            {
                _bound_tsignal = port->_bound_tsignal;
                _bound_tsignal->add_subscriber(this);
                break;
            }
        }
        if (!_bound_tsignal)
        {
            context()->logger->error("Input port {} could not be resolved to a signal", hier_name());
        }
    }

    template <typename T>
    const T &Input<T>::read() const
    {
        return _bound_tsignal->read();
    }

    //
    // OUTPUT<T>
    //

    template <typename T>
    Output<T>::Output(const std::string &name) : PortBase(name)
    {
    }

    template <typename T>
    Output<T>::Output(const std::string &name, Signal<T> &signal) : PortBase(name)
    {
        bind(signal);
    }

    template <typename T>
    Output<T>::operator Signal<T> &()
    {
        return *_bound_tsignal;
    }

    template <typename T>
    void Output<T>::bind(Signal<T> &signal)
    {
        if (_bound_tsignal)
        {
            context()->logger->error("Output port {} is already bound to a signal", hier_name());
        }
        _bound_tsignal = &signal;
        // _bound_signal = &signal;
        signal.add_driver(this);
        // signal.add_subscriber(this);
    }

    template <typename T>
    void Output<T>::bind(Output<T> &port)
    {
        _bound_ports.push_back(&port);
    }

    template <typename T>
    void Output<T>::finalize()
    {
        resolve();
    }

    template <typename T>
    void Output<T>::resolve()
    {
        if (_bound_tsignal)
        {
            return;
        }
        for (auto *port : _bound_ports)
        {
            port->resolve();
            if (port->_bound_tsignal)
            {
                _bound_tsignal = port->_bound_tsignal;
                break;
            }
        }
        if (!_bound_tsignal)
        {
            context()->logger->error("Output port {} could not be resolved to a signal", hier_name());
        }
    }

    template <typename T>
    void Output<T>::notify(EventType event)
    {
    }

    template <typename T>
    void Output<T>::write(const T &value)
    {
        _bound_tsignal->write(value);
        for (auto &port : _bound_ports)
        {
            port->write(value);
        }
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
