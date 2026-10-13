/*
    Signals and ports without the type of their value: what Python has for a value type that the framework has
    no class for, such as a struct of a model. One set of bindings serves every such type.

    SignalBase and PortBase read and write a value through its description (dspsim/value_type.h): the bytes
    of the value go to and from dspsim._values, which makes a numpy value of them (a numpy.record for a
    struct). The typed classes (SignalU8, InputS16, ...) have their own read() and write(), which hide these.

    SignalBaseArrayView and PortBaseArrayView are arrays of them: of the elements of an array of struct ports,
    or of an array field of a struct signal.
*/
#pragma once
#include <dspsim/port.h>
#include <dspsim/signal.h>
#include <dspsim/struct.h>
#include "nb_include.h"
#include "ndarray_bindings.h"

#include <cstddef>
#include <string>
#include <vector>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    using SignalBaseArrayView = detail::NdView<SignalBase>;
    using PortBaseArrayView = detail::NdView<PortBase>;

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

    /// sig["field"]: the signal of a field, or the array of signals of the elements of an array field. The
    /// field signals are owned by the context, like the signal of a bit selection.
    static inline nb::object signal_field(SignalBase &signal, const std::string &name)
    {
        // Only a struct has fields: for any other signal a name is the wrong type of key.
        const ValueType *type = signal.value_type();
        if (!type || type->kind != ValueType::Kind::structure || !type->shape.empty())
        {
            throw nb::type_error(("Signal " + signal.hier_name() + " has no fields: its value is not a struct with a DSPSIM_STRUCT").c_str());
        }
        try
        {
            auto elements = signal.field_array(name);
            if (elements.ndim() == 0)
            {
                return nb::cast(&elements.flat(0), nb::rv_policy::reference);
            }
            return nb::cast(std::move(elements));
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
            .def_prop_ro("q", &read_value<SignalBase>, nb::sig("def q(self, /) -> typing.Any"))
            .def_prop_ro("fields", [](const SignalBase &s)
                         {
                             std::vector<std::string> names;
                             const ValueType *type = s.value_type();
                             if (type && type->kind == ValueType::Kind::structure && type->shape.empty())
                             {
                                 for (auto &f : type->layout->fields)
                                 {
                                     names.push_back(f.name);
                                 }
                             }
                             return names; },
                         "The names of the fields of a signal whose value is a struct.");
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
            .def("signal", &PortBase::create_signal, nb::arg("name"),
                 "Create a signal that the port can bind to: of its value type and width.");
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
            .def_prop_ro("source", &FieldSignal<T>::source, nb::rv_policy::reference);
    }
} // namespace dspsim::bindings
