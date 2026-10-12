#pragma once
#include <dspsim/struct.h>
#include "nb_include.h"
#include "ndarray_bindings.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    /// The value types of the signals, ports and arrays that the framework binds.
    template <typename... Ts>
    struct StructValueTypes
    {
    };
    using StructFieldTypes = StructValueTypes<uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, double>;

    /// The Python object of a SignalStruct field, if the field is an X. A field that the struct owns is returned
    /// as that owner; a field that C++ code added by reference lives as long as the struct that `parent` wraps.
    template <typename X>
    static inline bool cast_signal_field(const SignalStruct::Field &field, nb::handle parent, nb::object &result)
    {
        if (field.type != typeid(X))
        {
            return false;
        }
        if (field.owner)
        {
            result = nb::cast(std::static_pointer_cast<X>(field.owner));
        }
        else
        {
            result = nb::cast(static_cast<X *>(field.object), nb::rv_policy::reference_internal, parent);
        }
        return true;
    }

    template <typename... Ts>
    static inline nb::object signal_field_object(const SignalStruct::Field &field, nb::handle parent, StructValueTypes<Ts...>)
    {
        nb::object result;
        if (cast_signal_field<SignalStruct>(field, parent, result) ||
            cast_signal_field<SignalStructArray>(field, parent, result) ||
            (cast_signal_field<Signal<Ts>>(field, parent, result) || ...) ||
            (cast_signal_field<SignalArray<Ts>>(field, parent, result) || ...))
        {
            return result;
        }
        throw nb::type_error(("Field '" + field.name + "' has a type without Python bindings").c_str());
    }

    /// Add a Python object to a SignalStruct as a field, if it is an X. The struct holds a reference to it.
    template <typename X>
    static inline bool add_signal_field(SignalStruct &signals, const std::string &name, nb::handle object)
    {
        if (!nb::isinstance<X>(object))
        {
            return false;
        }
        signals.add(name, nb::cast<std::shared_ptr<X>>(object));
        return true;
    }

    template <typename... Ts>
    static inline void add_signal_field(SignalStruct &signals, const std::string &name, nb::handle object, StructValueTypes<Ts...>)
    {
        if (nb::isinstance<SignalStruct>(object))
        {
            signals.add(name, nb::cast<SignalStruct>(object));
        }
        else if (nb::isinstance<SignalStructArray>(object))
        {
            signals.add(name, nb::cast<SignalStructArray>(object));
        }
        else if (!((add_signal_field<Signal<Ts>>(signals, name, object) || ...) ||
                   (add_signal_field<SignalArray<Ts>>(signals, name, object) || ...)))
        {
            throw nb::type_error("A field of a SignalStruct is a signal, a signal array, a SignalStruct or a SignalStructArray");
        }
    }

    /// The Python object of a PortStruct field, if the field is an X. It lives as long as the struct that
    /// `parent` wraps.
    template <typename X>
    static inline bool cast_port_field(const PortStruct::Field &field, nb::handle parent, nb::object &result)
    {
        if (field.type != typeid(X))
        {
            return false;
        }
        result = nb::cast(static_cast<X *>(field.object), nb::rv_policy::reference_internal, parent);
        return true;
    }

    template <typename... Ts>
    static inline nb::object port_field_object(const PortStruct::Field &field, nb::handle parent, StructValueTypes<Ts...>)
    {
        nb::object result;
        if (cast_port_field<PortStruct>(field, parent, result) ||
            cast_port_field<PortStructArray>(field, parent, result) ||
            (cast_port_field<Input<Ts>>(field, parent, result) || ...) ||
            (cast_port_field<Output<Ts>>(field, parent, result) || ...) ||
            (cast_port_field<InputArray<Ts>>(field, parent, result) || ...) ||
            (cast_port_field<OutputArray<Ts>>(field, parent, result) || ...))
        {
            return result;
        }
        throw nb::type_error(("Field '" + field.name + "' has a type without Python bindings").c_str());
    }

    /// Add a Python object to a PortStruct as a field, if it is an X.
    template <typename X>
    static inline bool add_port_field(PortStruct &ports, const std::string &name, nb::handle object)
    {
        if (!nb::isinstance<X>(object))
        {
            return false;
        }
        ports.add(name, nb::cast<X &>(object));
        return true;
    }

    template <typename... Ts>
    static inline void add_port_field(PortStruct &ports, const std::string &name, nb::handle object, StructValueTypes<Ts...>)
    {
        if (!(add_port_field<PortStruct>(ports, name, object) ||
              add_port_field<PortStructArray>(ports, name, object) ||
              (add_port_field<Input<Ts>>(ports, name, object) || ...) ||
              (add_port_field<Output<Ts>>(ports, name, object) || ...) ||
              (add_port_field<InputArray<Ts>>(ports, name, object) || ...) ||
              (add_port_field<OutputArray<Ts>>(ports, name, object) || ...)))
        {
            throw nb::type_error("A field of a PortStruct is an input, an output, an array of them, a PortStruct or a PortStructArray");
        }
    }

    template <typename Struct>
    static inline std::vector<std::string> struct_field_names(const Struct &s)
    {
        std::vector<std::string> names;
        for (auto &field : s.fields())
        {
            names.push_back(field.name);
        }
        return names;
    }

    // Fields are attributes (bus.data) and items (bus["data"]). An attribute of the class itself, such as
    // "name" or "bind", hides a field of that name, which is then only reachable as an item.
    static inline auto bind_signal_struct(nb::module_ &m, const char *name)
    {
        return nb::class_<SignalStruct>(m, name)
            .def(nb::init<std::string>(), nb::arg("name") = "")
            .def_prop_ro("name", &SignalStruct::name)
            .def_prop_ro("fields", &struct_field_names<SignalStruct>)
            .def("add", [](nb::handle self, const std::string &field, nb::handle object)
                 {
                     add_signal_field(nb::cast<SignalStruct &>(self), field, object, StructFieldTypes{});
                     return nb::borrow(self); }, nb::arg("field"), nb::arg("signal"),
                 nb::sig("def add(self, field: str, signal: object) -> SignalStruct"))
            .def("__contains__", [](const SignalStruct &self, const std::string &field)
                 { return self.find(field) != nullptr; })
            .def("__getitem__", [](nb::handle self, const std::string &field)
                 {
                     auto *f = nb::cast<SignalStruct &>(self).find(field);
                     if (!f)
                     {
                         throw nb::key_error(field.c_str());
                     }
                     return signal_field_object(*f, self, StructFieldTypes{}); }, nb::sig("def __getitem__(self, field: str, /) -> typing.Any"))
            .def("__getattr__", [](nb::handle self, const std::string &field)
                 {
                     auto &signals = nb::cast<SignalStruct &>(self);
                     auto *f = signals.find(field);
                     if (!f)
                     {
                         throw nb::attribute_error(("Signal struct '" + signals.name() + "' has no field '" + field + "'").c_str());
                     }
                     return signal_field_object(*f, self, StructFieldTypes{}); }, nb::sig("def __getattr__(self, field: str, /) -> typing.Any"));
    }

    static inline auto bind_port_struct(nb::module_ &m, const char *name)
    {
        return nb::class_<PortStruct>(m, name)
            .def(nb::init<std::string>(), nb::arg("name"))
            .def_prop_ro("name", &PortStruct::name)
            .def_prop_ro("fields", &struct_field_names<PortStruct>)
            // The struct refers to the port: keep the port alive as long as the struct.
            .def("add", [](nb::handle self, const std::string &field, nb::handle object)
                 {
                     add_port_field(nb::cast<PortStruct &>(self), field, object, StructFieldTypes{});
                     return nb::borrow(self); }, nb::arg("field"), nb::arg("port"), nb::keep_alive<1, 3>(),
                 nb::sig("def add(self, field: str, port: object) -> PortStruct"))
            .def("bind", nb::overload_cast<const SignalStruct &>(&PortStruct::bind), nb::arg("signals"))
            .def("bind", nb::overload_cast<PortStruct &>(&PortStruct::bind), nb::arg("ports"))
            .def("signals", &PortStruct::signals, nb::arg("name"))
            .def("__contains__", [](const PortStruct &self, const std::string &field)
                 { return self.find(field) != nullptr; })
            .def("__getitem__", [](nb::handle self, const std::string &field)
                 {
                     auto *f = nb::cast<PortStruct &>(self).find(field);
                     if (!f)
                     {
                         throw nb::key_error(field.c_str());
                     }
                     return port_field_object(*f, self, StructFieldTypes{}); }, nb::sig("def __getitem__(self, field: str, /) -> typing.Any"))
            .def("__getattr__", [](nb::handle self, const std::string &field)
                 {
                     auto &ports = nb::cast<PortStruct &>(self);
                     auto *f = ports.find(field);
                     if (!f)
                     {
                         throw nb::attribute_error(("Port struct '" + ports.name() + "' has no field '" + field + "'").c_str());
                     }
                     return port_field_object(*f, self, StructFieldTypes{}); }, nb::sig("def __getattr__(self, field: str, /) -> typing.Any"));
    }
    // One field of every struct of an array, as one array: a view of ports or signals, or an array of the nested
    // structs. The result refers to the structs, so it keeps `parent` alive.
    template <typename X>
    static inline nb::object struct_array_field(nb::handle parent, X &&view)
    {
        nb::object result = nb::cast(std::forward<X>(view), nb::rv_policy::move);
        nb::keep_alive_obj(result, parent);
        return result;
    }

    template <typename T>
    static inline bool port_struct_array_field(const PortStructArray &ports, const StructFieldType &type, const std::string &field, nb::handle parent, nb::object &result)
    {
        if (type == typeid(Input<T>) || type == typeid(InputArray<T>))
        {
            result = struct_array_field(parent, ports.input_array<T>(field));
            return true;
        }
        if (type == typeid(Output<T>) || type == typeid(OutputArray<T>))
        {
            result = struct_array_field(parent, ports.output_array<T>(field));
            return true;
        }
        return false;
    }

    template <typename... Ts>
    static inline nb::object port_struct_array_field(const PortStructArray &ports, const std::string &field, nb::handle parent, StructValueTypes<Ts...>)
    {
        // The type of the field is that of the first element. The view checks the others.
        auto *f = ports.size() > 0 ? ports.flat(0).find(field) : nullptr;
        if (!f)
        {
            throw nb::attribute_error(("Port struct array '" + ports.name() + "' has no field '" + field + "'").c_str());
        }
        nb::object result;
        if (f->type == typeid(PortStruct) || f->type == typeid(PortStructArray))
        {
            return struct_array_field(parent, ports.nested(field));
        }
        if ((port_struct_array_field<Ts>(ports, f->type, field, parent, result) || ...))
        {
            return result;
        }
        throw nb::type_error(("Field '" + field + "' has a type without Python bindings").c_str());
    }

    template <typename T>
    static inline bool signal_struct_array_field(const SignalStructArray &signals, const StructFieldType &type, const std::string &field, nb::handle parent, nb::object &result)
    {
        if (type == typeid(Signal<T>) || type == typeid(SignalArray<T>))
        {
            result = struct_array_field(parent, signals.array<T>(field));
            return true;
        }
        return false;
    }

    template <typename... Ts>
    static inline nb::object signal_struct_array_field(const SignalStructArray &signals, const std::string &field, nb::handle parent, StructValueTypes<Ts...>)
    {
        auto *f = signals.size() > 0 ? signals.flat(0).find(field) : nullptr;
        if (!f)
        {
            throw nb::attribute_error(("Signal struct array '" + signals.name() + "' has no field '" + field + "'").c_str());
        }
        nb::object result;
        if (f->type == typeid(SignalStruct) || f->type == typeid(SignalStructArray))
        {
            return struct_array_field(parent, signals.nested(field));
        }
        if ((signal_struct_array_field<Ts>(signals, f->type, field, parent, result) || ...))
        {
            return result;
        }
        throw nb::type_error(("Field '" + field + "' has a type without Python bindings").c_str());
    }

    // What an array of port structs and an array of signal structs have in common. Indexing is that of the other
    // arrays: ints for every dimension give the struct, fewer ints or slices a view of the array. A field name,
    // as an attribute or an item, gives that field of every struct as one array.
    template <typename Array, typename NbClass, typename FieldView>
    static inline void bind_struct_array_common(NbClass &cls, const char *name, FieldView field_view)
    {
        auto *item_sig = new std::string(std::string("def __getitem__(self, key: int | slice | str | tuple[int | slice, ...], /) -> typing.Any"));
        cls.def_prop_ro("name", &Array::name)
            .def_prop_ro("shape", [](const Array &a)
                         { return nb::tuple(nb::cast(a.shape())); })
            .def_prop_ro("ndim", [](const Array &a)
                         { return a.ndim(); })
            .def_prop_ro("size", [](const Array &a)
                         { return a.size(); })
            .def("__len__", [](const Array &a)
                 { return a.size(); })
            // The fields of the structs: those of the first one.
            .def_prop_ro("fields", [](const Array &a)
                         { return a.size() > 0 ? struct_field_names(a.flat(0)) : std::vector<std::string>{}; })
            .def("flat", [](Array &a, std::size_t i) -> auto &
                 { return a.flat(i); }, nb::arg("index"), nb::rv_policy::reference_internal)
            .def("__iter__", [](Array &a)
                 { return nb::make_iterator<nb::rv_policy::reference_internal>(nb::type<Array>(), "iterator", a.begin(), a.end()); }, nb::keep_alive<0, 1>())
            .def("__getitem__", [field_view](nb::handle self, nb::handle key) -> nb::object
                 {
                     auto &a = nb::cast<Array &>(self);
                     if (nb::isinstance<nb::str>(key))
                     {
                         return field_view(a, nb::cast<std::string>(key), self);
                     }
                     auto ranges = resolve_key(a, key);
                     if (!ranges)
                     {
                         throw nb::type_error("array indices must be integers, slices or the name of a field");
                     }
                     bool element = ranges->size() == a.ndim();
                     for (auto &r : *ranges)
                     {
                         element = element && r.drop;
                     }
                     if (!element)
                     {
                         return struct_array_field(self, a.select(*ranges));
                     }
                     Index idx;
                     for (auto &r : *ranges)
                     {
                         idx.push_back(static_cast<std::size_t>(r.start));
                     }
                     return nb::cast(&a.at(idx), nb::rv_policy::reference_internal, self); }, nb::arg("key"), nb::sig(item_sig->c_str()))
            .def("__getattr__", [field_view](nb::handle self, const std::string &field)
                 { return field_view(nb::cast<Array &>(self), field, self); }, nb::sig("def __getattr__(self, field: str, /) -> typing.Any"));
        (void)name;
    }

    static inline auto bind_signal_struct_array(nb::module_ &m, const char *name)
    {
        auto cls = nb::class_<SignalStructArray>(m, name)
                       // The structs in row-major order.
                       .def(nb::init<std::string, Shape, std::vector<SignalStruct>>(), nb::arg("name"), nb::arg("shape"), nb::arg("structs"));
        bind_struct_array_common<SignalStructArray>(cls, name, [](const SignalStructArray &a, const std::string &field, nb::handle self)
                                                    { return signal_struct_array_field(a, field, self, StructFieldTypes{}); });
        return cls;
    }

    static inline auto bind_port_struct_array(nb::module_ &m, const char *name)
    {
        auto cls = nb::class_<PortStructArray>(m, name)
                       // The structs in row-major order. The array refers to them: keep them alive with it.
                       .def("__init__", [](PortStructArray *self, std::string array_name, Shape shape, const std::vector<PortStruct *> &structs)
                            { new (self) PortStructArray(std::move(array_name), std::move(shape), structs); }, nb::arg("name"), nb::arg("shape"), nb::arg("structs"), nb::keep_alive<1, 4>())
                       .def("bind", nb::overload_cast<const SignalStructArray &>(&PortStructArray::bind), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const PortStructArray &>(&PortStructArray::bind), nb::arg("ports"))
                       .def("signals", &PortStructArray::signals, nb::arg("name"));
        bind_struct_array_common<PortStructArray>(cls, name, [](const PortStructArray &a, const std::string &field, nb::handle self)
                                                  { return port_struct_array_field(a, field, self, StructFieldTypes{}); });
        return cls;
    }
} // namespace dspsim::bindings
