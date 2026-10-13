#include <dspsim/port.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include <dspsim/derived_signal.h>

#include "log.h"

#include <cstring>
#include <stdexcept>
#include <string>

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
        if (Module *module = context()->active_module())
        {
            module->add_port(this);
        }
        else
        {
            DSPSIM_LOG_ERROR(context()->logger_, "A port cannot be declared at root level. Port: " + hier_name());
        }
    }

    PortBase::~PortBase() = default;

    int PortBase::width() const
    {
        return width_;
    }

    void PortBase::finalize()
    {
        // Finalize the port by resolving its bound signal and updating the bound signal's subscribers.
        resolve();

        // Associate the port's sensitivity events with the bound signal's events.
        update_bound_signal_subscribers();
    }

    void PortBase::sync()
    {
    }

    RecordPortBase *PortBase::record()
    {
        return nullptr;
    }

    RecordPortBase *RecordPortBase::record()
    {
        return this;
    }

    RecordSignalBase &RecordPortBase::bound_record() const
    {
        RecordSignalBase *signal = bound_signal() ? bound_signal()->record() : nullptr;
        if (!signal)
        {
            throw std::logic_error("Port " + hier_name() + " is not bound to a signal yet: its fields are those of the signal");
        }
        return *signal;
    }

    SignalBase &RecordPortBase::field(const std::string &name) const
    {
        return bound_record().field(name);
    }

    detail::NdView<SignalBase> RecordPortBase::field_array(const std::string &name) const
    {
        return bound_record().field_array(name);
    }

    std::vector<std::string> RecordPortBase::field_names() const
    {
        std::vector<std::string> names;
        const ValueType *type = value_type();
        if (type && type->kind == ValueType::Kind::structure && type->shape.empty())
        {
            for (auto &f : type->layout->fields)
            {
                names.push_back(f.name);
            }
        }
        return names;
    }

    const ValueType *PortBase::value_type() const
    {
        return nullptr;
    }

    const std::type_info &PortBase::value_typeid() const
    {
        return typeid(void);
    }

    bool PortBase::is_output() const
    {
        return kind() == "output";
    }

    std::shared_ptr<SignalBase> PortBase::create_signal(const std::string &) const
    {
        throw std::logic_error("Port " + hier_name() + " has no value type");
    }

    namespace
    {
        // Types are compared by name: a model's extension module and the framework each have a
        // std::type_info of a type, which do not compare equal with every standard library.
        bool same_value_type(const std::type_info &a, const std::type_info &b)
        {
            return a == b || std::strcmp(a.name(), b.name()) == 0;
        }
    } // namespace

    void PortBase::bind_any(SignalBase &signal)
    {
        if (!same_value_type(value_typeid(), signal.value_typeid()))
        {
            throw std::invalid_argument("Port " + hier_name() + " cannot bind to signal " + signal.hier_name() +
                                        ": the signal has another value type");
        }
        bind_base(signal);
    }

    void PortBase::bind_any(PortBase &port)
    {
        if (!same_value_type(value_typeid(), port.value_typeid()))
        {
            throw std::invalid_argument("Port " + hier_name() + " cannot bind to port " + port.hier_name() +
                                        ": the port has another value type");
        }
        if (is_output() != port.is_output())
        {
            throw std::invalid_argument("Port " + hier_name() + " cannot bind to port " + port.hier_name() +
                                        ": an input binds to an input and an output to an output");
        }
        bind_base(port);
    }

    const void *PortBase::value_data() const
    {
        if (!bound_signal_)
        {
            throw std::logic_error("Port " + hier_name() + " is not bound to a signal yet");
        }
        return bound_signal_->value_data();
    }

    void PortBase::write_data(const void *value)
    {
        if (!is_output())
        {
            throw std::logic_error("Port " + hier_name() + " is an input and cannot be written");
        }
        if (!bound_signal_)
        {
            throw std::logic_error("Port " + hier_name() + " is not bound to a signal yet");
        }
        bound_signal_->write_data(value);
    }

    void PortBase::bind_base(SignalBase &signal)
    {
        // Already bound.
        if (bound_signal_)
        {
            DSPSIM_LOG_ERROR(context()->logger_, "Port " + hier_name() + " is already bound to a signal");
        }

        // // Width mismatch check
        // if (signal.width() != this->width())
        // {
        //     // Error or warning?
        //     DSPSIM_LOG_WARN(context()->logger_, "Port " + hier_name() + " width mismatch with signal " + signal.hier_name());
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
            DSPSIM_LOG_ERROR(context()->logger_, "Port " + hier_name() + " is not bound to a signal");
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
    void Input<T>::bind(const BitSel &selection)
        requires std::is_integral_v<T>
    {
        this->bind_base(make_derived_signal<T>(*this, selection));
    }

    template <typename T>
    void Output<T>::bind(const BitSel &selection)
        requires std::is_integral_v<T>
    {
        this->bind_base(make_derived_signal<T>(*this, selection));
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
