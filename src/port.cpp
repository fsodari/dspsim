#include <dspsim/port.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include <dspsim/derived_signal.h>

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace dspsim
{
    namespace
    {
        // Create the derived signal that a port binds to in place of a bit selection.
        template <typename T>
        DerivedSignal<T> &make_derived_signal(const PortBase &port, const BitSel &selection)
        {
            if (selection.width() != port.width())
            {
                throw std::invalid_argument("Port " + port.hier_name() + " of width " + std::to_string(port.width()) +
                                            " cannot bind to a selection of width " + std::to_string(selection.width()));
            }
            return selection.signal<T>(port.name() + "_bitsel");
        }
    } // namespace

    PortBase::PortBase(const std::string &name, int width, const std::string &kind)
        : Model(name, kind),
          width_(width),
          static_change_event_(context()),
          static_posedge_event_(context()),
          static_negedge_event_(context())
    {
        // Associate the events with the internal events during construction.
        change_event_ = &static_change_event_;
        posedge_event_ = &static_posedge_event_;
        negedge_event_ = &static_negedge_event_;

        // Ports must only be declared within a module context.
        if (context()->_active_module())
        {
            context()->_active_module()->ports().push_back(this);
        }
        else
        {
            context()->logger->error("A port cannot be declared at root level. Port: {}", hier_name());
        }
    }

    void PortBase::finalize()
    {
        // Finalize the port by resolving its bound signal and updating the bound signal's subscribers.
        resolve();

        // Associate the port's sensitivity events with the bound signal's events.
        update_bound_signal_subscribers();
    }

    void PortBase::bind_base(SignalBase &signal)
    {
        // Already bound.
        if (bound_signal_)
        {
            context()->logger->error("Port {} is already bound to a signal", hier_name());
        }

        // // Width mismatch check
        // if (signal.width() != this->width())
        // {
        //     // Error or warning?
        //     context()->logger->warn("Port {} width mismatch with signal {}", hier_name(), signal.hier_name());
        // }

        bound_signal_ = &signal;
    }

    void PortBase::bind_base(PortBase &port)
    {
        // TODO: Give an error if attempting to bind ports at the same hierarchical level.
        bound_ports_.push_back(&port);
    }

    void PortBase::resolve()
    {
        // This port has already been bound/resolved.
        if (bound_signal_)
        {
            return;
        }
        // If the port has not been bound, recursive search through connected ports to find a bound signal.
        for (auto *port : bound_ports_)
        {
            port->resolve();
            if (port->bound_signal())
            {
                bound_signal_ = port->bound_signal();
                break;
            }
        }
        // If no bound signal was found after searching all connected ports,
        // the port is unconnected.
        if (!bound_signal_)
        {
            context()->logger->error("Port {} is not bound to a signal", hier_name());
        }
    }

    void PortBase::update_bound_signal_subscribers()
    {
        // If binding failed, this will be a nullptr.
        // This is an error condition, but it should fail gracefully at the end of elaboration.
        if (bound_signal_)
        {
            bound_signal_->pos().merge_static_subscribers(static_posedge_event_);
            bound_signal_->neg().merge_static_subscribers(static_negedge_event_);
            bound_signal_->change().merge_static_subscribers(static_change_event_);

            // Set dynamic sensitivity to the signal's dynamic events.
            change_event_ = &bound_signal_->change();
            posedge_event_ = &bound_signal_->pos();
            negedge_event_ = &bound_signal_->neg();
        }
    }

    template <typename T>
    Input<T>::Input(const std::string &name, int width) : PortBase(name, width, "input")
    {
        // Register an input port with the parent module.
        if (context()->_active_module())
        {
            context()->_active_module()->inputs().push_back(this);
        }
    }

    template <typename T>
    void Input<T>::bind(const BitSel &selection)
        requires std::is_integral_v<T>
    {
        bind_base(make_derived_signal<T>(*this, selection));
    }

    template <typename T>
    Output<T>::Output(const std::string &name, int width) : PortBase(name, width, "output")
    {
        if (context()->_active_module())
        {
            context()->_active_module()->outputs().push_back(this);
        }
    }

    template <typename T>
    void Output<T>::bind(const BitSel &selection)
        requires std::is_integral_v<T>
    {
        bind_base(make_derived_signal<T>(*this, selection));
    }

    template class Input<uint8_t>;
    template class Input<uint16_t>;
    template class Input<uint32_t>;
    template class Input<uint64_t>;
    template class Input<int8_t>;
    template class Input<int16_t>;
    template class Input<int32_t>;
    template class Input<int64_t>;
    template class Input<float>;
    template class Input<double>;

    template class Output<uint8_t>;
    template class Output<uint16_t>;
    template class Output<uint32_t>;
    template class Output<uint64_t>;
    template class Output<int8_t>;
    template class Output<int16_t>;
    template class Output<int32_t>;
    template class Output<int64_t>;
    template class Output<float>;
    template class Output<double>;
}
