#include <dspsim/signal.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/derived_signal.h>
#include <dspsim/struct.h>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace dspsim
{
    SignalBase::SignalBase(const std::string &name, int width)
        : SignalBase(Context::obtain().get(), name, width)
    {
    }

    SignalBase::SignalBase(Context *context, const std::string &name, int width)
        : Model(context, name, "signal"),
          width_(width),
          dependents_(),
          scheduled_(false),
          change_event_(context),
          posedge_event_(context),
          negedge_event_(context)
    {
        context->add_signal(this);
    }

    RecordSignalBase *SignalBase::record()
    {
        return nullptr;
    }

    RecordSignalBase *RecordSignalBase::record()
    {
        return this;
    }

    BitSel SignalBase::slice(int hi, int lo)
    {
        return BitSel(*this, hi, lo);
    }

    BitSel SignalBase::operator[](BitRange range)
    {
        return slice(range.hi, range.lo);
    }

    BitSel SignalBase::operator[](int bit)
    {
        return slice(bit, bit);
    }

    const BitSel *SignalBase::source_selection() const
    {
        return derived_ ? derived_->source_selection() : nullptr;
    }

    detail::FieldRoot SignalBase::derived_field_root()
    {
        return derived_->field_root();
    }

    void SignalBase::write_through(const void *value)
    {
        derived_->write_through(value);
    }

    namespace
    {
        // The field `name` of the struct that is the value of a signal.
        const StructField &struct_field(const SignalBase &signal, const std::string &name)
        {
            const ValueType *type = signal.value_type();
            if (!type || type->kind != ValueType::Kind::structure || !type->shape.empty())
            {
                throw std::invalid_argument("Signal " + signal.hier_name() +
                                            " has no fields: its value is not a struct with a DSPSIM_STRUCT");
            }
            const StructField *field = type->layout->find(name);
            if (!field)
            {
                throw std::invalid_argument("The value of signal " + signal.hier_name() + " (" + type->layout->name +
                                            ") has no field '" + name + "'");
            }
            return *field;
        }

        // Whether a field signal has the value type of one element of a field. A struct and its first field
        // are at the same place, so the place alone does not say which of them a field signal is a view of.
        // Layouts are compared by name: every module has its own copy of the layout of a struct.
        bool same_element(const ValueType &signal, const ValueType &field)
        {
            if (signal.kind != field.kind || signal.size != field.size || !signal.shape.empty())
            {
                return false;
            }
            return signal.kind != ValueType::Kind::structure || signal.layout->name == field.layout->name;
        }
    } // namespace

    std::vector<std::string> RecordSignalBase::field_names() const
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

    SignalBase &RecordSignalBase::field(const std::string &name)
    {
        const StructField &f = struct_field(*this, name);
        if (!f.type.shape.empty())
        {
            throw std::invalid_argument("Field '" + name + "' of signal " + hier_name() +
                                        " is an array: its elements are signals (field_array())");
        }
        return field_array(name).flat(0);
    }

    detail::NdView<SignalBase> RecordSignalBase::field_array(const std::string &name)
    {
        const StructField &f = struct_field(*this, name);
        const detail::FieldRoot root = field_root();
        if (!root.signal)
        {
            throw std::logic_error("Signal " + hier_name() + " is a view of a bit selection and has no fields");
        }
        const std::size_t count = std::accumulate(f.type.shape.begin(), f.type.shape.end(), std::size_t{1}, std::multiplies<>());
        // The names of the signals, which are only needed for those that do not exist yet: a field is asked
        // for every time it is read or written from Python.
        std::vector<std::string> names;

        std::vector<SignalBase *> elements;
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::size_t offset = f.offset + i * f.type.size;
            const detail::FieldRoot element{root.signal, static_cast<char *>(root.d) + offset, static_cast<const char *>(root.q) + offset};

            // The view of this element, if it was asked for before.
            SignalBase *signal = nullptr;
            for (auto *dependent : root.signal->dependents_)
            {
                SignalBase *candidate = dependent->field_signal(element.q);
                if (candidate && candidate->value_type() && same_element(*candidate->value_type(), f.type))
                {
                    signal = candidate;
                    break;
                }
            }
            if (!signal)
            {
                if (names.empty())
                {
                    names = detail::struct_element_names(this->name() + "." + name, f.type.shape);
                }
                signal = &f.make_signal(names[i], element);
            }
            elements.push_back(signal);
        }
        return detail::NdView<SignalBase>(f.type.shape, std::move(elements));
    }

    namespace detail
    {
        void check_field_root(const SignalBase &signal, const FieldRoot &root)
        {
            if (!root.signal)
            {
                throw std::logic_error("Signal " + signal.hier_name() + " is a view of a bit selection and has no fields");
            }
        }

        std::string field_signal_name(const SignalBase &signal, std::size_t offset)
        {
            const ValueType *type = signal.value_type();
            if (type && type->kind == ValueType::Kind::structure && type->shape.empty())
            {
                for (auto &f : type->layout->fields)
                {
                    if (f.offset == offset)
                    {
                        return signal.name() + "." + f.name;
                    }
                }
            }
            return signal.name() + ".field";
        }
    } // namespace detail

    void SignalBase::request_update()
    {
        context()->signal_update_stack_.push_back(this);
    }

    void SignalBase::schedule_dependents()
    {
        for (auto *dependent : dependents_)
        {
            context()->post_update_stack_.push_back(dependent);
        }
    }

    void SignalBase::refresh_dependents()
    {
        for (auto *dependent : dependents_)
        {
            dependent->refresh();
        }
    }

    void SignalBase::add_dependent(DerivedSignalBase *dependent)
    {
        if (std::find(dependents_.begin(), dependents_.end(), dependent) == dependents_.end())
        {
            dependents_.push_back(dependent);
            has_dependents_ = true;
        }
    }

    // The member functions of Signal<T> are defined in the header.
    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int8_t>;
    template class Signal<int16_t>;
    template class Signal<int32_t>;
    template class Signal<int64_t>;
    template class Signal<float>;
    template class Signal<double>;
    template class Signal<bool>;
}
