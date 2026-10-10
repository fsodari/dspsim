#pragma once
#include <dspsim/bits.h>
#include <dspsim/bitsel.h>
#include <dspsim/derived_signal.h>
#include "nb_include.h"

#include <string>
#include <vector>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    // A python int of any size or sign as 64 raw bits (two's complement, truncated), like assigning it to a 64-bit logic.
    static inline uint64_t int_to_bits(const nb::int_ &value)
    {
        return PyLong_AsUnsignedLongLongMask(value.ptr());
    }

    static inline void check_bit_width(int width, int min_width)
    {
        if (width < min_width || width > 64)
        {
            throw nb::value_error(("width must be in [" + std::to_string(min_width) + ", 64]").c_str());
        }
    }

    // Convert a python key, sig[bit] or sig[hi:lo] (inclusive, as in SystemVerilog), to a bit range.
    // An omitted hi or lo selects to the end: sig[:8] is sig[width-1:8] and sig[7:] is sig[7:0].
    static inline BitRange resolve_bit_key(nb::handle key, int width)
    {
        if (nb::isinstance<nb::int_>(key))
        {
            int bit = nb::cast<int>(key);
            return {bit, bit};
        }
        if (nb::isinstance<nb::slice>(key))
        {
            nb::object start = key.attr("start");
            nb::object stop = key.attr("stop");
            if (!key.attr("step").is_none())
            {
                throw nb::value_error("bit slices do not support a step");
            }
            int hi = start.is_none() ? width - 1 : nb::cast<int>(start);
            int lo = stop.is_none() ? 0 : nb::cast<int>(stop);
            return {hi, lo};
        }
        throw nb::type_error("bits are selected with an int or a slice [hi:lo]");
    }

    // Append a pack() argument: a selection, a signal, or an iterable (array, view, list) of them.
    static inline void pack_append(std::vector<BitSel> &sels, nb::handle item)
    {
        BitSel *sel = nullptr;
        if (nb::try_cast<BitSel *>(item, sel, false) && sel)
        {
            sels.push_back(*sel);
            return;
        }
        SignalBase *signal = nullptr;
        if (nb::try_cast<SignalBase *>(item, signal, false) && signal)
        {
            sels.emplace_back(*signal);
            return;
        }
        if (!nb::isinstance<nb::str>(item) && nb::hasattr(item, "__iter__"))
        {
            for (nb::handle element : nb::iter(item))
            {
                pack_append(sels, element);
            }
            return;
        }
        throw nb::type_error("pack() arguments must be signals, bit selections, or iterables of them");
    }

    static inline auto bind_bitsel(nb::module_ &m, const char *name)
    {
        auto write = [](const BitSel &s, const nb::int_ &value)
        { s.write(int_to_bits(value)); };
        auto cls = nb::class_<BitSel>(m, name)
                       .def(nb::init<SignalBase &>(), nb::arg("signal"))
                       .def(nb::init<SignalBase &, int, int>(), nb::arg("signal"), nb::arg("hi"), nb::arg("lo"))
                       .def_prop_ro("width", &BitSel::width)
                       // (signal, hi, lo) tuples, least significant first.
                       .def_prop_ro("parts", [](const BitSel &s)
                                    {
                                        nb::list parts;
                                        for (const auto &p : s.parts())
                                        {
                                            parts.append(nb::make_tuple(nb::cast(p.signal, nb::rv_policy::reference), p.hi, p.lo));
                                        }
                                        return parts; })
                       .def("read", &BitSel::read)
                       .def("read_signed", &BitSel::read_signed)
                       .def("write", write, nb::arg("value"))
                       .def_prop_rw("value", &BitSel::read, write, nb::arg("value"))
                       .def("__int__", &BitSel::read)
                       .def("slice", &BitSel::slice, nb::arg("hi"), nb::arg("lo"))
                       .def("__getitem__", [](const BitSel &s, nb::handle key)
                            {
                                auto range = resolve_bit_key(key, s.width());
                                return s.slice(range.hi, range.lo); }, nb::arg("key"), nb::sig("def __getitem__(self, key: int | slice, /) -> BitSel"))
                       .def("signal", [](const BitSel &s, const std::string &name) -> SignalBase &
                            { return s.signal(name); }, nb::arg("name") = "", nb::rv_policy::reference)
                       .def("__repr__", [](const BitSel &s)
                            {
                                // Most significant first, as in SystemVerilog {a, b}.
                                std::string parts;
                                for (auto p = s.parts().rbegin(); p != s.parts().rend(); ++p)
                                {
                                    parts += (parts.empty() ? "" : ", ") + p->signal->name() + "[" + std::to_string(p->hi) + ":" + std::to_string(p->lo) + "]";
                                }
                                return "BitSel({" + parts + "}, width=" + std::to_string(s.width()) + ")"; });

        // Signals can be used wherever a selection is expected, e.g. to bind a port of a different type.
        nb::implicitly_convertible<SignalBase, BitSel>();
        return cls;
    }

    // pack() and the bit helpers from <dspsim/bits.h>. Values are python ints, truncated to 64 bits.
    static inline void bind_bit_functions(nb::module_ &m)
    {
        m.def("pack", [](nb::args args)
              {
                  std::vector<BitSel> sels;
                  for (nb::handle arg : args)
                  {
                      pack_append(sels, arg);
                  }
                  return BitSel::concat(sels); },
              "Concatenate signals, selections and arrays, most significant first, as in SystemVerilog {a, b}.");
        m.def("mask", [](int width)
              {
                  check_bit_width(width, 0);
                  return mask(width); }, nb::arg("width"));
        m.def("bits", [](const nb::int_ &value, int hi, int lo)
              {
                  check_bit_range(hi, lo, 64, "a 64-bit value");
                  return bits(int_to_bits(value), hi, lo); }, nb::arg("value"), nb::arg("hi"), nb::arg("lo"));
        m.def("zext", [](const nb::int_ &value, int width)
              {
                  check_bit_width(width, 0);
                  return zext(int_to_bits(value), width); }, nb::arg("value"), nb::arg("width"));
        m.def("sext", [](const nb::int_ &value, int width)
              {
                  check_bit_width(width, 1);
                  return sext(int_to_bits(value), width); }, nb::arg("value"), nb::arg("width"));
    }

    template <typename T>
    static inline auto bind_derived_signal(nb::module_ &m, const char *name)
    {
        return nb::class_<DerivedSignal<T>, Signal<T>>(m, name)
            .def_prop_ro("selection", &DerivedSignal<T>::selection);
    }
} // namespace dspsim::bindings
