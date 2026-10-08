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
    // Ints become single-element ranges that drop their dimension. Returns nullopt for unsupported key types.
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
                ranges.push_back({static_cast<std::ptrdiff_t>(start), static_cast<std::ptrdiff_t>(step), count, false});
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

    // Python type names for an array or view class, from its registered name ("Input16Array" or "InputArrayView16").
    struct ArrayNames
    {
        std::string element, view;

        explicit ArrayNames(const std::string &name)
        {
            std::string kind, suffix;
            if (auto pos = name.find("ArrayView"); pos != std::string::npos)
            {
                kind = name.substr(0, pos);
                suffix = name.substr(pos + 9);
            }
            else
            {
                auto base = name.substr(0, name.size() - 5); // "Array"
                auto cut = base.find_first_of("0123456789F");
                kind = base.substr(0, cut);
                suffix = base.substr(cut);
            }
            element = kind + suffix;
            view = kind + "ArrayView" + suffix;
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
        auto *view_sig = new std::string("def __getitem__(self, key: slice | int | tuple[int | slice, ...], /) -> " + names.view);
        auto *slice_sig = new std::string("def slice(self, key: slice | int | tuple[int | slice, ...] = ()) -> " + names.view);
        auto get_view = [](Array &a, nb::handle key)
        {
            auto ranges = resolve_key(a, key);
            if (!ranges)
                throw nb::type_error("array indices must be integers or slices");
            return a.select(*ranges);
        };
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
            .def("flat", [](Array &a, std::size_t i) -> auto &
                 { return a.flat(i); }, nb::arg("index"), nb::rv_policy::reference_internal)
            .def("at", [](Array &a, const Index &idx) -> auto &
                 { return a.at(idx); }, nb::arg("index"), nb::rv_policy::reference_internal)
            .def("__iter__", [](Array &a)
                 { return nb::make_iterator<nb::rv_policy::reference_internal>(nb::type<Array>(), "iterator", a.begin(), a.end()); },
                 nb::keep_alive<0, 1>())
            // Keys made only of ints are matched by type (no exceptions are used for dispatch, which would be slow).
            // A full-rank key returns the element. A partial key falls back to a view with those dimensions dropped.
            .def("__getitem__", [](Array &a, const std::variant<std::ptrdiff_t, std::vector<std::ptrdiff_t>> &key) -> nb::object
                 {
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
                         return nb::cast(a.select(ranges));
                     Index idx;
                     for (auto &r : ranges)
                         idx.push_back(static_cast<std::size_t>(r.start));
                     return nb::cast(&a.at(idx), nb::rv_policy::reference); },
                 nb::arg("key"), nb::keep_alive<0, 1>(),
                 nb::sig(elem_sig->c_str()))
            .def("__getitem__", get_view, nb::arg("key"), nb::keep_alive<0, 1>(),
                 nb::sig(view_sig->c_str()))
            // Always returns a view, so it skips the int-key overload above. An empty key views the whole array.
            .def("slice", get_view, nb::arg("key") = nb::tuple(), nb::keep_alive<0, 1>(),
                 nb::sig(slice_sig->c_str()));
    }

} // namespace dspsim::bindings
