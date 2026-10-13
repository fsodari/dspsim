#pragma once
#include <dspsim/ndarray.h>
#include <optional>
#include <variant>
#include <nanobind/stl/variant.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    // Members shared by every array class. Shapes and indices are python tuples.
    // Convert a python key (int, slice, or a tuple of them) to one Range per indexed dimension.
    // Ints become single-element ranges that drop their dimension. Slices with a step other than 1 raise ValueError.
    // Returns nullopt for unsupported key types.
    template <typename Array>
    static inline std::optional<std::vector<Range>> resolve_key(const Array &a, nb::handle key)
    {
        nb::tuple items = nb::isinstance<nb::tuple>(key) ? nb::borrow<nb::tuple>(key) : nb::make_tuple(key);
        if (items.size() > a.ndim())
            throw nb::index_error("too many indices for array");
        std::vector<Range> ranges;
        for (std::size_t d = 0; d < items.size(); ++d)
        {
            nb::handle item = items[d];
            if (nb::isinstance<nb::slice>(item))
            {
                auto [start, stop, step, count] = nb::borrow<nb::slice>(item).compute(a.extent(d));
                if (step != 1)
                    throw nb::value_error("array slices do not support a step");
                ranges.push_back({static_cast<std::ptrdiff_t>(start), count, false});
            }
            else if (nb::isinstance<nb::int_>(item))
            {
                ranges.push_back(Slice::at(nb::cast<std::ptrdiff_t>(item)).resolve(a.extent(d)));
            }
            else
                return std::nullopt;
        }
        return ranges;
    }

    // Python type names for an array or view class, from its registered name ("InputArrayU16" or "InputArrayViewU16").
    struct ArrayNames
    {
        std::string element, view;

        explicit ArrayNames(const std::string &name)
        {
            auto pos = name.find("Array");
            auto kind = name.substr(0, pos);
            auto suffix = name.substr(pos + 5);
            if (suffix.rfind("View", 0) == 0)
                suffix = suffix.substr(4);
            element = kind + suffix;
            view = kind + "ArrayView" + suffix;
        }
    };

    // Nested python lists with the shape of the array, with get(i) providing the i-th element value in row-major order.
    template <typename Get>
    static inline nb::object to_nested(const Shape &shape, std::size_t dim, std::size_t &flat, Get &get)
    {
        if (dim == shape.size())
            return nb::cast(get(flat++));
        nb::list out;
        for (std::size_t i = 0; i < shape[dim]; ++i)
            out.append(to_nested(shape, dim + 1, flat, get));
        return std::move(out);
    }

    // Flatten a scalar (broadcast to every element) or nested sequences matching the shape into row-major values.
    template <typename V>
    static inline void from_nested(nb::handle value, const Shape &shape, std::size_t dim, std::vector<V> &out)
    {
        if (dim == shape.size())
        {
            out.push_back(nb::cast<V>(value));
            return;
        }
        if (!nb::hasattr(value, "__len__"))
            throw nb::value_error("value must be a scalar or a nested sequence matching the array shape");
        std::size_t n = 0;
        for (auto item : nb::iter(value))
        {
            if (n++ >= shape[dim])
                break;
            from_nested(item, shape, dim + 1, out);
        }
        if (n != shape[dim])
            throw nb::value_error("sequence length does not match the array shape");
    }

    // Copy the values into a new numpy array with the shape of the array or view.
    template <typename V, typename Array, typename Get>
    static inline nb::ndarray<nb::numpy, V> to_numpy(const Array &a, Get get)
    {
        auto *data = new V[a.size() ? a.size() : 1];
        std::size_t i = 0;
        for (auto &e : a)
            data[i++] = get(e);
        nb::capsule owner(data, [](void *p) noexcept
                          { delete[] static_cast<V *>(p); });
        return nb::ndarray<nb::numpy, V>(data, a.ndim(), a.shape().data(), owner);
    }

    template <typename V, typename Array>
    static inline nb::ndarray<nb::numpy, V> to_numpy(const Array &a)
    {
        return to_numpy<V>(a, [](const auto &e)
                           { return e.read(); });
    }

    // Read/write accessors, added only where the element type supports them (Signal, Input, Output).
    template <typename Array, typename NbClass>
    static inline void bind_ndarray_values(NbClass &cls)
    {
        using Elem = typename Array::element_type;

        // An element without read() (a signal or port without the type of its value) has neither.
        if constexpr (detail::Readable<Elem>)
        {
            using V = detail::value_of<Elem>;
            auto read = [](const Array &a)
            {
                return to_numpy<V>(a);
            };
            cls.def("to_numpy", [](const Array &a)
                    { return to_numpy<V>(a); })
                // Lets np.asarray(array) work. dtype and copy are handled by numpy after conversion.
                .def("__array__", [](const Array &a, nb::handle, nb::handle)
                     { return to_numpy<V>(a); }, nb::arg("dtype") = nb::none(), nb::arg("copy") = nb::none())
                .def("read", read)
                .def_prop_ro("value", read, nb::rv_policy::move)
                .def_prop_ro("q", read, nb::rv_policy::move);
        }
        if constexpr (detail::Writable<Elem>)
        {
            using V = detail::value_of<Elem>;
            // numpy arrays (or anything convertible with the same shape). A 0-d array is broadcast.
            auto write_np = [](Array &a, const nb::ndarray<const V, nb::c_contig, nb::device::cpu> &arr)
            {
                if (arr.ndim() == 0)
                {
                    a.write(arr.data()[0]);
                    return;
                }
                if (arr.ndim() != a.ndim())
                    throw nb::value_error("array shape does not match");
                for (std::size_t d = 0; d < a.ndim(); ++d)
                    if (arr.shape(d) != a.shape()[d])
                        throw nb::value_error("array shape does not match");
                std::size_t i = 0;
                for (auto &e : a)
                    e.write(arr.data()[i++]);
            };
            auto write = [write_np](Array &a, nb::handle value)
            {
                nb::ndarray<const V, nb::c_contig, nb::device::cpu> arr;
                if (nb::try_cast(value, arr))
                {
                    write_np(a, arr);
                    return;
                }
                std::vector<V> values;
                if (a.ndim() > 0 && !nb::hasattr(value, "__len__"))
                {
                    a.write(nb::cast<V>(value));
                    return;
                }
                values.reserve(a.size());
                from_nested(value, a.shape(), 0, values);
                a.write(values);
            };
            cls.def("write", write, nb::arg("value"))
                .def_prop_rw(
                    "value", [](const Array &a)
                    { return to_numpy<V>(a); },
                    write, nb::rv_policy::move, nb::arg("value"));
            // "d" is the value that will be visible on the next update. Only present for Signal and Output elements.
            if constexpr (requires(const Elem &e) { e.read_d_(); })
                cls.def_prop_rw(
                    "d", [](const Array &a)
                    { return to_numpy<V>(a, [](const auto &e)
                                         { return e.read_d_(); }); },
                    write, nb::rv_policy::move, nb::arg("value"));
        }
    }

    /// How an element of an array becomes a Python object that refers to the array. The default is the
    /// class that nanobind has for the type of the element. An array whose elements are base classes
    /// (SignalBase, PortBase) specializes it to give each element the most specific class that it has.
    template <typename Elem>
    struct ElementCaster
    {
        static constexpr bool custom = false;

        static nb::object cast(Elem *element, nb::handle parent)
        {
            return nb::cast(element, nb::rv_policy::reference_internal, parent);
        }
    };

    // Members shared by every array and array view class. Indexing with ints and slices (like numpy) is supported:
    // all ints returns the element, otherwise a view with the int dimensions dropped.
    template <typename Array, typename NbClass>
    static inline void bind_ndarray_common(NbClass &cls, const char *class_name)
    {
        // nb::sig keeps the pointers, so the strings must stay alive.
        ArrayNames names{class_name};
        auto *elem_sig = new std::string("def __getitem__(self, key: int | tuple[int, ...], /) -> " + names.element);
        // The stub for these two overloads is written by the pattern in src/_framework.pat (it adds the type-ignore
        // comment their intended overlap needs): keep the two in sync.
        auto *view_sig = new std::string("def __getitem__(self, key: slice | int | tuple[int | slice, ...], /) -> " + names.view);
        auto *slice_sig = new std::string("def slice(self, key: slice | int | tuple[int | slice, ...] = ()) -> " + names.view);
        auto *flat_sig = new std::string("def flat(self, index: int) -> " + names.element);
        auto *at_sig = new std::string("def at(self, index: collections.abc.Sequence[int]) -> " + names.element);
        auto *iter_sig = new std::string("def __iter__(self) -> collections.abc.Iterator[" + names.element + "]");
        using Elem = typename Array::element_type;
        auto get_view = [](Array &a, nb::handle key)
        {
            auto ranges = resolve_key(a, key);
            if (!ranges)
                throw nb::type_error("array indices must be integers or slices");
            return a.select(*ranges);
        };
        bind_ndarray_values<Array>(cls);
        cls.def_prop_ro("shape", [](const Array &a)
                        { return nb::tuple(nb::cast(a.shape())); })
            .def_prop_ro("ndim", [](const Array &a)
                         { return a.ndim(); })
            .def("extent", [](const Array &a, std::size_t dim)
                 { return a.extent(dim); }, nb::arg("dim"))
            .def("__len__", [](const Array &a)
                 { return a.size(); })
            .def_prop_ro("size", [](const Array &a)
                         { return a.size(); })
            .def("flat", [](nb::handle self, std::size_t i)
                 { return ElementCaster<Elem>::cast(&nb::cast<Array &>(self).flat(i), self); }, nb::arg("index"),
                 nb::sig(flat_sig->c_str()))
            .def("at", [](nb::handle self, const Index &idx)
                 { return ElementCaster<Elem>::cast(&nb::cast<Array &>(self).at(idx), self); }, nb::arg("index"),
                 nb::sig(at_sig->c_str()))
            .def("__iter__", [](nb::handle self) -> nb::object
                 {
                     auto &a = nb::cast<Array &>(self);
                     if constexpr (ElementCaster<Elem>::custom)
                     {
                         nb::list elements;
                         for (auto &element : a)
                         {
                             elements.append(ElementCaster<Elem>::cast(&element, self));
                         }
                         return nb::iter(elements);
                     }
                     else
                     {
                         nb::object iterator = nb::make_iterator<nb::rv_policy::reference_internal>(nb::type<Array>(), "iterator", a.begin(), a.end());
                         nb::keep_alive_obj(iterator, self);
                         return iterator;
                     } }, nb::sig(iter_sig->c_str()))
            // Keys made only of ints are matched by type (no exceptions are used for dispatch, which would be slow).
            // A full-rank key returns the element. A partial key falls back to a view with those dimensions dropped.
            // Both refer to the array. The element is returned with reference_internal and not with a keep_alive
            // policy: an element that Python owns (a port that a Python struct was made of) already has its
            // Python object, which must not be made to keep the array alive, or the two keep each other.
            .def("__getitem__", [](nb::handle self, const std::variant<std::ptrdiff_t, std::vector<std::ptrdiff_t>> &key) -> nb::object
                 {
                     auto &a = nb::cast<Array &>(self);
                     std::vector<std::ptrdiff_t> ints;
                     if (auto *i = std::get_if<std::ptrdiff_t>(&key))
                         ints.push_back(*i);
                     else
                         ints = std::get<std::vector<std::ptrdiff_t>>(key);
                     if (ints.size() > a.ndim())
                         throw nb::index_error("too many indices for array");

                     std::vector<Range> ranges;
                     for (std::size_t d = 0; d < ints.size(); ++d)
                         ranges.push_back(Slice::at(ints[d]).resolve(a.extent(d)));

                     if (ints.size() < a.ndim())
                     {
                         nb::object view = nb::cast(a.select(ranges));
                         nb::keep_alive_obj(view, self);
                         return view;
                     }
                     Index idx;
                     for (auto &r : ranges)
                         idx.push_back(static_cast<std::size_t>(r.start));
                     return ElementCaster<Elem>::cast(&a.at(idx), self); }, nb::arg("key"), nb::sig(elem_sig->c_str()))
            .def("__getitem__", get_view, nb::arg("key"), nb::keep_alive<0, 1>(), nb::sig(view_sig->c_str()))
            // Always returns a view, so it skips the int-key overload above. An empty key views the whole array.
            .def("slice", get_view, nb::arg("key") = nb::tuple(), nb::keep_alive<0, 1>(), nb::sig(slice_sig->c_str()));
    }

} // namespace dspsim::bindings
