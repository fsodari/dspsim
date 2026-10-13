/*
    Signals and ports without the type of their value: what Python has for a value type that the framework has
    no class for, such as a struct of a model. One set of bindings serves every such type.

    SignalBase and PortBase read and write a value through its description (dspsim/value_type.h): the bytes
    of the value go to and from dspsim._values, which makes a numpy value of them (a numpy.record for a
    struct). The typed classes (SignalU8, InputS16, ...) have their own read() and write(), which hide these.

    A signal or port whose value is a struct is a RecordSignal or a RecordPort (RecordSignalBase,
    RecordPortBase): the fields of the struct are attributes, each the signal of that field, and they come
    before the attributes that every signal has, so that a field called `id` or `width` is the field.

    SignalBaseArrayView and PortBaseArrayView are arrays of them: of the elements of an array of struct ports,
    or of an array field of a struct signal.
*/
#pragma once
#include <dspsim/port.h>
#include <dspsim/signal.h>
#include <dspsim/struct.h>
#include "nb_include.h"
#include "ndarray_bindings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    using SignalBaseArrayView = detail::NdView<SignalBase>;
    using PortBaseArrayView = detail::NdView<PortBase>;

    /// The Python object of a signal: of the class of its value type if it has one (SignalU8, FieldSignalS16,
    /// ...), a RecordSignal if its value is a struct, a SignalBase otherwise. nanobind finds the first by the
    /// type of the object and falls back to the type of the pointer, which this chooses.
    static inline nb::object signal_object(SignalBase *signal, nb::rv_policy policy = nb::rv_policy::reference, nb::handle parent = nb::handle())
    {
        if (RecordSignalBase *record = signal->record())
        {
            return nb::cast(record, policy, parent);
        }
        return nb::cast(signal, policy, parent);
    }

    static inline nb::object signal_object(const std::shared_ptr<SignalBase> &signal)
    {
        if (RecordSignalBase *record = signal->record())
        {
            return nb::cast(std::shared_ptr<RecordSignalBase>(signal, record));
        }
        return nb::cast(signal);
    }

    /// The Python object of a port: see signal_object().
    static inline nb::object port_object(PortBase *port, nb::rv_policy policy = nb::rv_policy::reference, nb::handle parent = nb::handle())
    {
        if (RecordPortBase *record = port->record())
        {
            return nb::cast(record, policy, parent);
        }
        return nb::cast(port, policy, parent);
    }

    template <>
    struct ElementCaster<SignalBase>
    {
        static constexpr bool custom = true;

        static nb::object cast(SignalBase *element, nb::handle parent)
        {
            return signal_object(element, nb::rv_policy::reference_internal, parent);
        }
    };

    template <>
    struct ElementCaster<PortBase>
    {
        static constexpr bool custom = true;

        static nb::object cast(PortBase *element, nb::handle parent)
        {
            return port_object(element, nb::rv_policy::reference_internal, parent);
        }
    };

    /// dspsim._values: the numpy side of the conversions.
    static inline nb::module_ values_module()
    {
        return nb::module_::import_("dspsim._values");
    }

    static inline nb::tuple shape_tuple(const Shape &shape)
    {
        nb::list items;
        for (auto n : shape)
        {
            items.append(n);
        }
        return nb::tuple(items);
    }

    /// The description of a value type that dspsim._values takes: nested tuples.
    static inline nb::object value_description(const ValueType &type)
    {
        using Kind = ValueType::Kind;
        if (type.kind != Kind::structure)
        {
            const char *kind = type.kind == Kind::boolean            ? "b"
                               : type.kind == Kind::unsigned_integer ? "u"
                               : type.kind == Kind::signed_integer   ? "i"
                                                                     : "f";
            return nb::make_tuple(kind, type.size, shape_tuple(type.shape));
        }
        nb::list fields;
        for (auto &f : type.layout->fields)
        {
            fields.append(nb::make_tuple(f.name, f.offset, value_description(f.type)));
        }
        return nb::make_tuple("S", type.layout->name, type.size, nb::tuple(fields), shape_tuple(type.shape));
    }

    /// The description of the value type of a signal or port. Throws TypeError if it has none.
    template <typename Model>
    static inline const ValueType &described_type(const Model &model)
    {
        const ValueType *type = model.value_type();
        if (!type)
        {
            throw nb::type_error((model.hier_name() + " has a value that Python cannot read or write: "
                                                      "its C++ type is a struct without a DSPSIM_STRUCT")
                                     .c_str());
        }
        return *type;
    }

    /// Bytes of a whole value: all elements of an array.
    static inline std::size_t value_bytes(const ValueType &type)
    {
        std::size_t bytes = type.size;
        for (auto n : type.shape)
        {
            bytes *= n;
        }
        return bytes;
    }

    template <typename Model>
    static inline nb::object value_dtype(const Model &model)
    {
        return values_module().attr("dtype_of")(value_description(described_type(model)));
    }

    /// The value of a signal or port as a Python object.
    template <typename Model>
    static inline nb::object read_value(const Model &model)
    {
        const ValueType &type = described_type(model);
        nb::bytes data(static_cast<const char *>(model.value_data()), value_bytes(type));
        return values_module().attr("from_bytes")(value_description(type), data);
    }

    /// The pending value of a signal: what was written last.
    static inline nb::object read_pending(const SignalBase &signal)
    {
        const ValueType &type = described_type(signal);
        nb::bytes data(static_cast<const char *>(signal.pending_data()), value_bytes(type));
        return values_module().attr("from_bytes")(value_description(type), data);
    }

    /// Write a Python object to a signal or port.
    template <typename Model>
    static inline void write_value(Model &model, nb::handle value)
    {
        const ValueType &type = described_type(model);
        nb::bytes data = nb::borrow<nb::bytes>(values_module().attr("to_bytes")(value_description(type), value));
        if (data.size() != value_bytes(type))
        {
            throw nb::value_error("the value does not have the size of the value type");
        }
        model.write_data(data.c_str());
    }

    /// The values of an array of signals or ports as one numpy array: the shape of the array, then that of a
    /// value. All elements have the value type of the first.
    template <typename Array>
    static inline nb::object read_values(const Array &array)
    {
        if (array.size() == 0)
        {
            throw nb::value_error("the array has no elements");
        }
        const ValueType &type = described_type(array.flat(0));
        const std::size_t bytes = value_bytes(type);
        std::string data;
        data.reserve(bytes * array.size());
        for (auto &element : array)
        {
            data.append(static_cast<const char *>(element.value_data()), bytes);
        }
        return values_module().attr("from_bytes")(value_description(type), nb::bytes(data.data(), data.size()), shape_tuple(array.shape()));
    }

    template <typename Array>
    static inline void write_values(Array &array, nb::handle value)
    {
        if (array.size() == 0)
        {
            return;
        }
        const ValueType &type = described_type(array.flat(0));
        const std::size_t bytes = value_bytes(type);
        nb::bytes data = nb::borrow<nb::bytes>(values_module().attr("to_bytes")(value_description(type), value, shape_tuple(array.shape())));
        if (data.size() != bytes * array.size())
        {
            throw nb::value_error("the value does not have the size of the array");
        }
        std::size_t i = 0;
        for (auto &element : array)
        {
            element.write_data(data.c_str() + bytes * i++);
        }
    }

    /// A field of a struct signal or port: the signal of the field, or the array of the signals of the
    /// elements of a field that is an array. The field signals are owned by the context, like the signal of a
    /// bit selection. Throws std::invalid_argument if there is no such field.
    template <typename Record>
    static inline nb::object record_field(Record &record, const std::string &name)
    {
        auto elements = record.field_array(name);
        if (elements.ndim() == 0)
        {
            return signal_object(&elements.flat(0));
        }
        return nb::cast(std::move(elements));
    }

    /// Whether a struct signal or port has a field of that name.
    template <typename Record>
    static inline bool has_record_field(const Record &record, std::string_view name)
    {
        const ValueType *type = record.value_type();
        return type && type->kind == ValueType::Kind::structure && type->shape.empty() && type->layout->find(name);
    }

    /// __getattribute__ of a struct signal or port: a field comes before the attributes of the class, except
    /// those in `api`, which are what a struct signal or port is used with. So `bus.id` is the field `id` of
    /// the struct and not the id that every model has, while `bus.read` is always the method; a field called
    /// `read` is an item, bus["read"].
    template <typename Record, std::size_t N>
    static inline nb::object record_attribute(nb::handle self, nb::handle name, const std::array<std::string_view, N> &api)
    {
        const std::string attribute = nb::cast<std::string>(name);
        const bool reserved = attribute.starts_with("__") || std::find(api.begin(), api.end(), attribute) != api.end();
        if (!reserved)
        {
            auto &record = nb::cast<Record &>(self);
            if (has_record_field(record, attribute))
            {
                try
                {
                    return record_field(record, attribute);
                }
                catch (const std::logic_error &e)
                {
                    // A port that has no signal yet.
                    throw nb::attribute_error(e.what());
                }
            }
        }
        PyObject *result = PyObject_GenericGetAttr(self.ptr(), name.ptr());
        if (!result)
        {
            throw nb::python_error();
        }
        return nb::steal(result);
    }

    /// record["field"]: the same as the attribute, for a field whose name is taken.
    template <typename Record>
    static inline nb::object record_item(Record &record, const std::string &name)
    {
        try
        {
            return record_field(record, name);
        }
        catch (const std::invalid_argument &e)
        {
            throw nb::key_error(e.what());
        }
    }

    /// The members of SignalBase for a value without its type. `cls` is the class of SignalBase.
    template <typename NbClass>
    static inline void bind_signal_values(NbClass &cls)
    {
        // The classes of the numbers (SignalU8, ...) have their own read(), write() and value, with the types
        // of their values. The signatures here say Any, so that those are valid overrides in the stub.
        cls.def_prop_ro("dtype", &value_dtype<SignalBase>, "The numpy dtype of the value.")
            .def("read", &read_value<SignalBase>, nb::sig("def read(self) -> typing.Any"))
            .def("write", &write_value<SignalBase>, nb::arg("value"), nb::sig("def write(self, value: typing.Any) -> None"))
            .def_prop_rw("value", &read_value<SignalBase>, &write_value<SignalBase>,
                         nb::for_getter(nb::sig("def value(self, /) -> typing.Any")),
                         nb::for_setter(nb::sig("def value(self, value: typing.Any, /) -> None")))
            // "d" is the value that was written last and that the next update makes visible.
            .def_prop_rw("d", &read_pending, &write_value<SignalBase>,
                         nb::for_getter(nb::sig("def d(self, /) -> typing.Any")),
                         nb::for_setter(nb::sig("def d(self, value: typing.Any, /) -> None")))
            .def_prop_ro("q", &read_value<SignalBase>, nb::sig("def q(self, /) -> typing.Any"));
    }

    /// RecordSignal: a signal whose value is a struct. The fields are attributes and items.
    static inline auto bind_record_signal(nb::module_ &m, const char *name)
    {
        // What a struct signal is used with: these names are never fields as attributes.
        static constexpr std::array<std::string_view, 10> api{"read", "write", "value", "q", "d", "dtype", "fields", "change", "name", "hier_name"};
        return nb::class_<RecordSignalBase, SignalBase>(m, name)
            .def_prop_ro("fields", &RecordSignalBase::field_names, "The names of the fields.")
            .def("__getattribute__", [](nb::handle self, nb::handle attribute)
                 { return record_attribute<RecordSignalBase>(self, attribute, api); },
                 nb::sig("def __getattribute__(self, name: str, /) -> typing.Any"))
            // The key is the name of a field. The signature also has the keys of SignalBase, the bits of a
            // number, which a struct does not have: an override must take what the base class takes.
            .def("__getitem__", [](RecordSignalBase &record, nb::handle key)
                 {
                     if (!nb::isinstance<nb::str>(key))
                     {
                         throw nb::type_error("The key of a struct signal is the name of a field");
                     }
                     return record_item(record, nb::cast<std::string>(key)); }, nb::arg("key"),
                 nb::sig("def __getitem__(self, key: str | int | slice, /) -> typing.Any"))
            .def("__contains__", [](const RecordSignalBase &record, const std::string &field)
                 { return has_record_field(record, field); });
    }

    /// The members of PortBase for a value without its type.
    template <typename NbClass>
    static inline void bind_port_values(NbClass &cls)
    {
        // Bind to a signal or to a port, of the value type of this port.
        auto bind = [](PortBase &port, nb::handle target)
        {
            if (nb::isinstance<SignalBase>(target))
            {
                port.bind_any(nb::cast<SignalBase &>(target));
            }
            else if (nb::isinstance<PortBase>(target))
            {
                port.bind_any(nb::cast<PortBase &>(target));
            }
            else
            {
                throw nb::type_error("A port binds to a signal or to a port");
            }
        };
        // The classes of the numbers (InputU8, ...) have their own members of these names, with the types of
        // their values. The signatures here say Any, so that those are valid overrides in the stub.
        cls.def_prop_ro("dtype", &value_dtype<PortBase>, "The numpy dtype of the value.")
            .def_prop_ro("is_output", &PortBase::is_output)
            .def("read", &read_value<PortBase>, nb::sig("def read(self) -> typing.Any"))
            .def("write", &write_value<PortBase>, nb::arg("value"), nb::sig("def write(self, value: typing.Any) -> None"))
            // Read only, as for an input: an output is written with write().
            .def_prop_ro("value", &read_value<PortBase>, nb::sig("def value(self, /) -> typing.Any"))
            .def_prop_ro("q", &read_value<PortBase>, nb::sig("def q(self, /) -> typing.Any"))
            .def("bind", bind, nb::arg("target"), nb::sig("def bind(self, target: typing.Any, /) -> None"))
            .def("__call__", bind, nb::arg("target"), nb::sig("def __call__(self, target: typing.Any, /) -> None"))
            .def("signal", [](const PortBase &port, const std::string &name)
                 { return signal_object(port.create_signal(name)); }, nb::arg("name"),
                 nb::sig("def signal(self, name: str) -> typing.Any"),
                 "Create a signal that the port can bind to: of its value type and width.");
    }

    /// RecordPort: a port whose value is a struct. The fields are those of the signal that it is bound to.
    static inline auto bind_record_port(nb::module_ &m, const char *name)
    {
        // What a struct port is used with: these names are never fields as attributes.
        static constexpr std::array<std::string_view, 12> api{"read", "write", "value", "q", "dtype", "fields", "change", "name",
                                                              "hier_name", "bind", "signal", "is_output"};
        return nb::class_<RecordPortBase, PortBase>(m, name)
            .def_prop_ro("fields", &RecordPortBase::field_names, "The names of the fields.")
            .def("__getattribute__", [](nb::handle self, nb::handle attribute)
                 { return record_attribute<RecordPortBase>(self, attribute, api); },
                 nb::sig("def __getattribute__(self, name: str, /) -> typing.Any"))
            .def("__getitem__", &record_item<RecordPortBase>, nb::arg("field"),
                 nb::sig("def __getitem__(self, field: str, /) -> typing.Any"))
            .def("__contains__", [](const RecordPortBase &record, const std::string &field)
                 { return has_record_field(record, field); });
    }

    static inline void check_same_shape(const Shape &a, const Shape &b)
    {
        if (a != b)
        {
            throw nb::value_error("the arrays do not have the same shape");
        }
    }

    static inline auto bind_signal_base_array(nb::module_ &m, const char *name)
    {
        using Array = SignalBaseArrayView;
        auto cls = nb::class_<Array>(m, name)
                       .def_prop_ro("dtype", [](const Array &a)
                                    { return value_dtype(a.flat(0)); })
                       .def("read", &read_values<Array>)
                       .def("write", &write_values<Array>, nb::arg("value"))
                       .def_prop_rw("value", &read_values<Array>, &write_values<Array>, nb::arg("value"))
                       // Written like the other signals: array.d = values.
                       .def_prop_rw("d", &read_values<Array>, &write_values<Array>, nb::arg("value"))
                       .def_prop_ro("q", &read_values<Array>)
                       .def("to_numpy", &read_values<Array>)
                       .def("__array__", [](const Array &a, nb::handle, nb::handle)
                            { return read_values(a); }, nb::arg("dtype") = nb::none(), nb::arg("copy") = nb::none());
        bind_ndarray_common<Array>(cls, name);
        return cls;
    }

    static inline auto bind_port_base_array(nb::module_ &m, const char *name)
    {
        using Array = PortBaseArrayView;
        auto cls = nb::class_<Array>(m, name)
                       .def_prop_ro("dtype", [](const Array &a)
                                    { return value_dtype(a.flat(0)); })
                       .def("read", &read_values<Array>)
                       .def("write", &write_values<Array>, nb::arg("value"))
                       .def_prop_rw("value", &read_values<Array>, &write_values<Array>, nb::arg("value"))
                       .def_prop_ro("q", &read_values<Array>)
                       .def("to_numpy", &read_values<Array>)
                       .def("__array__", [](const Array &a, nb::handle, nb::handle)
                            { return read_values(a); }, nb::arg("dtype") = nb::none(), nb::arg("copy") = nb::none())
                       .def("bind", [](Array &a, const SignalBaseArrayView &signals)
                            {
                                check_same_shape(a.shape(), signals.shape());
                                for (std::size_t i = 0; i < a.size(); ++i)
                                {
                                    a.flat(i).bind_any(signals.flat(i));
                                } }, nb::arg("signals"))
                       .def("bind", [](Array &a, const Array &ports)
                            {
                                check_same_shape(a.shape(), ports.shape());
                                for (std::size_t i = 0; i < a.size(); ++i)
                                {
                                    a.flat(i).bind_any(ports.flat(i));
                                } }, nb::arg("ports"))
                       .def("signals", [](const Array &a, const std::string &name)
                            {
                                // The signals are owned by the context, like those that Signal.create() makes.
                                std::vector<SignalBase *> signals;
                                std::size_t i = 0;
                                for (auto &element_name : detail::struct_element_names(name, a.shape()))
                                {
                                    signals.push_back(a.flat(i++).create_signal(element_name).get());
                                }
                                return SignalBaseArrayView(a.shape(), std::move(signals)); }, nb::arg("name"),
                            "Create the signals that the ports can bind to, as an array of the same shape.");
        bind_ndarray_common<Array>(cls, name);
        return cls;
    }

    template <typename T>
    static inline auto bind_field_signal(nb::module_ &m, const char *name)
    {
        return nb::class_<FieldSignal<T>, Signal<T>>(m, name)
            .def_prop_ro("source", [](const FieldSignal<T> &field)
                         { return signal_object(&field.source()); }, nb::sig("def source(self, /) -> SignalBase"));
    }
} // namespace dspsim::bindings
