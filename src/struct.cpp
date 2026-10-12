#include <dspsim/struct.h>

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace dspsim
{
    namespace
    {
        // Path of a field for an error message: "outer.inner".
        std::string struct_field_path(const std::string &path, const std::string &field)
        {
            return path.empty() ? field : path + "." + field;
        }

        template <typename Fields>
        auto find_struct_field(Fields &fields, const std::string &field) -> decltype(&fields[0])
        {
            for (auto &f : fields)
            {
                if (f.name == field)
                {
                    return &f;
                }
            }
            return nullptr;
        }

        [[noreturn]] void throw_struct_bind_errors(const std::string &ports, const std::string &other, const std::vector<std::string> &errors)
        {
            std::string message = "Cannot bind port struct '" + ports + "' to '" + other + "':";
            for (auto &error : errors)
            {
                message += "\n  " + error;
            }
            throw std::invalid_argument(message);
        }

        std::size_t struct_array_size(const Shape &shape)
        {
            std::size_t size = 1;
            for (auto extent : shape)
            {
                size *= extent;
            }
            return size;
        }

        // Throws std::invalid_argument unless an array of structs of that shape has that many structs.
        void check_struct_array_size(const std::string &name, const Shape &shape, std::size_t elements)
        {
            if (struct_array_size(shape) != elements)
            {
                throw std::invalid_argument("Struct array '" + name + "' has " + std::to_string(elements) + " structs, which is not what its shape holds");
            }
        }

        std::string struct_shape_text(const Shape &shape)
        {
            std::string text = "(";
            for (std::size_t d = 0; d < shape.size(); ++d)
            {
                text += (d ? ", " : "") + std::to_string(shape[d]);
            }
            return text + ")";
        }

        template <typename Struct>
        std::vector<Struct *> struct_pointers(std::vector<Struct> &structs)
        {
            std::vector<Struct *> pointers;
            pointers.reserve(structs.size());
            for (auto &s : structs)
            {
                pointers.push_back(&s);
            }
            return pointers;
        }

        // The struct, or the structs of the array, that one field is in every element of an array of structs,
        // with the shape of that array appended to shape.
        template <typename Struct, typename StructArray, typename Structs>
        std::vector<Struct *> nested_struct_elements(const Structs &structs, const std::string &field, Shape &shape)
        {
            return detail::struct_field_elements<Struct, StructArray>(structs, field, shape);
        }

        const PortStruct::Ops struct_array_ops{
            &typeid(SignalStructArray),
            [](void *port, void *signal)
            {
                static_cast<PortStructArray *>(port)->bind(*static_cast<SignalStructArray *>(signal));
            },
            [](void *port, void *other)
            {
                static_cast<PortStructArray *>(port)->bind(*static_cast<PortStructArray *>(other));
            },
            [](const void *port, SignalStruct &signals, const std::string &field, const std::string &name)
            {
                signals.add(field, static_cast<const PortStructArray *>(port)->signals(name));
            }};

        const PortStruct::Ops nested_struct_ops{
            &typeid(SignalStruct),
            [](void *port, void *signal)
            {
                static_cast<PortStruct *>(port)->bind(*static_cast<SignalStruct *>(signal));
            },
            [](void *port, void *other)
            {
                static_cast<PortStruct *>(port)->bind(*static_cast<PortStruct *>(other));
            },
            [](const void *port, SignalStruct &signals, const std::string &field, const std::string &name)
            {
                signals.add(field, static_cast<const PortStruct *>(port)->signals(name));
            }};
    } // namespace

    namespace detail
    {
        std::vector<std::string> struct_element_names(const std::string &name, const Shape &shape)
        {
            std::vector<std::string> names;
            const std::size_t size = struct_array_size(shape);
            names.reserve(size);
            Index idx(shape.size(), 0);
            for (std::size_t flat = 0; flat < size; ++flat)
            {
                std::string element_name = name;
                for (auto i : idx)
                {
                    element_name += "[" + std::to_string(i) + "]";
                }
                names.push_back(std::move(element_name));
                for (std::size_t d = shape.size(); d-- > 0;)
                {
                    if (++idx[d] < shape[d])
                    {
                        break;
                    }
                    idx[d] = 0;
                }
            }
            return names;
        }

        void throw_no_struct_field(const std::string &name, const std::string &field)
        {
            throw std::invalid_argument("Struct '" + name + "' has no field '" + field + "'");
        }

        void throw_struct_field_type(const std::string &name, const std::string &field)
        {
            throw std::invalid_argument("Field '" + field + "' of struct '" + name + "' has another type");
        }

        void throw_struct_field_shape(const std::string &name, const std::string &field)
        {
            throw std::invalid_argument("Field '" + field + "' of struct '" + name + "' has another shape than in the other structs of the array");
        }
    } // namespace detail

    SignalStruct::SignalStruct(std::string name) : name_(std::move(name))
    {
    }

    const std::string &SignalStruct::name() const
    {
        return name_;
    }

    const std::vector<SignalStruct::Field> &SignalStruct::fields() const
    {
        return fields_;
    }

    const SignalStruct::Field *SignalStruct::find(const std::string &field) const
    {
        return find_struct_field(fields_, field);
    }

    SignalStruct &SignalStruct::add(const std::string &field, SignalStruct signals)
    {
        auto owner = std::make_shared<SignalStruct>(std::move(signals));
        auto *object = owner.get();
        return add_field({field, typeid(SignalStruct), object, std::move(owner)});
    }

    SignalStruct &SignalStruct::add(const std::string &field, SignalStructArray signals)
    {
        auto owner = std::make_shared<SignalStructArray>(std::move(signals));
        auto *object = owner.get();
        return add_field({field, typeid(SignalStructArray), object, std::move(owner)});
    }

    SignalStruct &SignalStruct::nested(const std::string &field) const
    {
        return *static_cast<SignalStruct *>(get(field, typeid(SignalStruct)));
    }

    SignalStructArray &SignalStruct::struct_array(const std::string &field) const
    {
        return *static_cast<SignalStructArray *>(get(field, typeid(SignalStructArray)));
    }

    SignalStruct &SignalStruct::add_field(Field field)
    {
        if (find(field.name))
        {
            throw std::invalid_argument("Signal struct '" + name_ + "' already has a field '" + field.name + "'");
        }
        fields_.push_back(std::move(field));
        return *this;
    }

    void *SignalStruct::get(const std::string &field, std::type_index type) const
    {
        auto *f = find(field);
        if (!f)
        {
            throw std::invalid_argument("Signal struct '" + name_ + "' has no field '" + field + "'");
        }
        if (f->type != type)
        {
            throw std::invalid_argument("Field '" + field + "' of signal struct '" + name_ + "' has another type");
        }
        return f->object;
    }

    SignalStructArray::SignalStructArray(std::string name, Shape shape, std::vector<SignalStruct> elements)
        : SignalStructArray(std::move(name), std::move(shape), std::make_shared<std::vector<SignalStruct>>(std::move(elements)))
    {
    }

    const std::string &SignalStructArray::name() const
    {
        return name_;
    }

    SignalStructArray SignalStructArray::view() const
    {
        return *this;
    }

    SignalStructArray SignalStructArray::slice(const Slices &slices) const
    {
        return {name_, detail::NdView<SignalStruct>::slice(slices), storage_};
    }

    SignalStructArray SignalStructArray::select(const std::vector<Range> &ranges) const
    {
        return {name_, detail::NdView<SignalStruct>::select(ranges), storage_};
    }

    SignalStructArray SignalStructArray::nested(const std::string &field) const
    {
        Shape shape = this->shape();
        std::vector<SignalStruct> structs;
        for (auto *nested : nested_struct_elements<SignalStruct, SignalStructArray>(*this, field, shape))
        {
            // A copy refers to the same signals.
            structs.push_back(*nested);
        }
        return {name_ + "." + field, std::move(shape), std::move(structs)};
    }

    SignalStructArray::SignalStructArray(std::string name, Shape shape, Storage storage)
        : detail::NdView<SignalStruct>(std::move(shape), struct_pointers(*storage)), name_(std::move(name)), storage_(std::move(storage))
    {
        check_struct_array_size(name_, this->shape(), size());
    }

    SignalStructArray::SignalStructArray(std::string name, detail::NdView<SignalStruct> view, Storage storage)
        : detail::NdView<SignalStruct>(std::move(view)), name_(std::move(name)), storage_(std::move(storage))
    {
    }

    PortStruct::PortStruct(std::string name) : name_(std::move(name))
    {
    }

    const std::string &PortStruct::name() const
    {
        return name_;
    }

    const std::vector<PortStruct::Field> &PortStruct::fields() const
    {
        return fields_;
    }

    const PortStruct::Field *PortStruct::find(const std::string &field) const
    {
        return find_struct_field(fields_, field);
    }

    void PortStruct::bind(const SignalStruct &signals)
    {
        std::vector<std::string> errors;
        check(signals, "", errors);
        if (!errors.empty())
        {
            throw_struct_bind_errors(name_, signals.name(), errors);
        }
        bind_checked(signals);
    }

    void PortStruct::bind(PortStruct &ports)
    {
        std::vector<std::string> errors;
        check(ports, "", errors);
        if (!errors.empty())
        {
            throw_struct_bind_errors(name_, ports.name(), errors);
        }
        bind_checked(ports);
    }

    SignalStruct PortStruct::signals(const std::string &name) const
    {
        SignalStruct result{name};
        for (auto &f : fields_)
        {
            f.ops->add_signal(f.object, result, f.name, name + "." + f.name);
        }
        return result;
    }

    PortStruct &PortStruct::nested(const std::string &field) const
    {
        return *static_cast<PortStruct *>(get(field, typeid(PortStruct)));
    }

    PortStructArray &PortStruct::struct_array(const std::string &field) const
    {
        return *static_cast<PortStructArray *>(get(field, typeid(PortStructArray)));
    }

    void PortStruct::add(const std::string &field, PortStruct &ports)
    {
        add_field({field, typeid(PortStruct), &ports, &nested_struct_ops});
    }

    void PortStruct::add(const std::string &field, PortStructArray &ports)
    {
        add_field({field, typeid(PortStructArray), &ports, &struct_array_ops});
    }

    void PortStruct::add_field(Field field)
    {
        if (find(field.name))
        {
            throw std::invalid_argument("Port struct '" + name_ + "' already has a field '" + field.name + "'");
        }
        fields_.push_back(std::move(field));
    }

    void *PortStruct::get(const std::string &field, std::type_index type) const
    {
        auto *f = find(field);
        if (!f)
        {
            throw std::invalid_argument("Port struct '" + name_ + "' has no field '" + field + "'");
        }
        if (f->type != type)
        {
            throw std::invalid_argument("Field '" + field + "' of port struct '" + name_ + "' has another type");
        }
        return f->object;
    }

    void PortStruct::check(const SignalStruct &signals, const std::string &path, std::vector<std::string> &errors) const
    {
        for (auto &f : fields_)
        {
            auto field_path = struct_field_path(path, f.name);
            auto *other = signals.find(f.name);
            if (!other)
            {
                errors.push_back("no field '" + field_path + "' in the signals");
            }
            else if (other->type != *f.ops->signal_type)
            {
                errors.push_back("field '" + field_path + "' of the signals does not have the type of the port");
            }
            else if (f.type == typeid(PortStruct))
            {
                static_cast<const PortStruct *>(f.object)->check(*static_cast<const SignalStruct *>(other->object), field_path, errors);
            }
            else if (f.type == typeid(PortStructArray))
            {
                static_cast<const PortStructArray *>(f.object)->check(*static_cast<const SignalStructArray *>(other->object), field_path, errors);
            }
        }
        for (auto &other : signals.fields())
        {
            if (!find(other.name))
            {
                errors.push_back("no field '" + struct_field_path(path, other.name) + "' in the ports");
            }
        }
    }

    void PortStruct::check(const PortStruct &ports, const std::string &path, std::vector<std::string> &errors) const
    {
        for (auto &f : fields_)
        {
            auto field_path = struct_field_path(path, f.name);
            auto *other = ports.find(f.name);
            if (!other)
            {
                errors.push_back("no field '" + field_path + "' in the other ports");
            }
            else if (other->type != f.type)
            {
                errors.push_back("field '" + field_path + "' of the other ports has another type");
            }
            else if (f.type == typeid(PortStruct))
            {
                static_cast<const PortStruct *>(f.object)->check(*static_cast<const PortStruct *>(other->object), field_path, errors);
            }
            else if (f.type == typeid(PortStructArray))
            {
                static_cast<const PortStructArray *>(f.object)->check(*static_cast<const PortStructArray *>(other->object), field_path, errors);
            }
        }
        for (auto &other : ports.fields())
        {
            if (!find(other.name))
            {
                errors.push_back("no field '" + struct_field_path(path, other.name) + "' in the ports");
            }
        }
    }

    void PortStruct::bind_checked(const SignalStruct &signals)
    {
        for (auto &f : fields_)
        {
            f.ops->bind_signal(f.object, signals.find(f.name)->object);
        }
    }

    void PortStruct::bind_checked(PortStruct &ports)
    {
        for (auto &f : fields_)
        {
            f.ops->bind_port(f.object, ports.find(f.name)->object);
        }
    }

    PortStructArray::PortStructArray(std::string name, Shape shape, std::vector<PortStruct *> elements)
        : detail::NdView<PortStruct>(std::move(shape), std::move(elements)), name_(std::move(name))
    {
        check_struct_array_size(name_, this->shape(), size());
    }

    const std::string &PortStructArray::name() const
    {
        return name_;
    }

    PortStructArray PortStructArray::view() const
    {
        return *this;
    }

    PortStructArray PortStructArray::slice(const Slices &slices) const
    {
        return {name_, detail::NdView<PortStruct>::slice(slices)};
    }

    PortStructArray PortStructArray::select(const std::vector<Range> &ranges) const
    {
        return {name_, detail::NdView<PortStruct>::select(ranges)};
    }

    void PortStructArray::bind(const SignalStructArray &signals)
    {
        std::vector<std::string> errors;
        check(signals, "", errors);
        if (!errors.empty())
        {
            throw_struct_bind_errors(name_, signals.name(), errors);
        }
        bind_checked(signals);
    }

    void PortStructArray::bind(const PortStructArray &ports)
    {
        std::vector<std::string> errors;
        check(ports, "", errors);
        if (!errors.empty())
        {
            throw_struct_bind_errors(name_, ports.name(), errors);
        }
        bind_checked(ports);
    }

    SignalStructArray PortStructArray::signals(const std::string &name) const
    {
        std::vector<SignalStruct> structs;
        structs.reserve(size());
        std::size_t i = 0;
        for (auto &element_name : detail::struct_element_names(name, shape()))
        {
            structs.push_back(flat(i++).signals(element_name));
        }
        return {name, shape(), std::move(structs)};
    }

    PortStructArray PortStructArray::nested(const std::string &field) const
    {
        Shape shape = this->shape();
        auto structs = nested_struct_elements<PortStruct, PortStructArray>(*this, field, shape);
        return {name_ + "." + field, std::move(shape), std::move(structs)};
    }

    PortStructArray::PortStructArray(std::string name, detail::NdView<PortStruct> view)
        : detail::NdView<PortStruct>(std::move(view)), name_(std::move(name))
    {
    }

    void PortStructArray::check(const SignalStructArray &signals, const std::string &path, std::vector<std::string> &errors) const
    {
        if (signals.shape() != shape())
        {
            errors.push_back("array '" + path + "' has the shape " + struct_shape_text(shape()) + " in the ports and " + struct_shape_text(signals.shape()) + " in the signals");
            return;
        }
        auto names = detail::struct_element_names(path, shape());
        for (std::size_t i = 0; i < size(); ++i)
        {
            flat(i).check(signals.flat(i), names[i], errors);
        }
    }

    void PortStructArray::check(const PortStructArray &ports, const std::string &path, std::vector<std::string> &errors) const
    {
        if (ports.shape() != shape())
        {
            errors.push_back("array '" + path + "' has the shape " + struct_shape_text(shape()) + " in the ports and " + struct_shape_text(ports.shape()) + " in the other ports");
            return;
        }
        auto names = detail::struct_element_names(path, shape());
        for (std::size_t i = 0; i < size(); ++i)
        {
            flat(i).check(ports.flat(i), names[i], errors);
        }
    }

    void PortStructArray::bind_checked(const SignalStructArray &signals)
    {
        for (std::size_t i = 0; i < size(); ++i)
        {
            flat(i).bind_checked(signals.flat(i));
        }
    }

    void PortStructArray::bind_checked(const PortStructArray &ports)
    {
        for (std::size_t i = 0; i < size(); ++i)
        {
            flat(i).bind_checked(ports.flat(i));
        }
    }
} // namespace dspsim
