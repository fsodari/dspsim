# Structs as Values

A signal carries a value of any type that can be copied, compared with `==` and constructed without
arguments. With a struct as its value, one signal carries all the fields: a write replaces the whole value,
the signal is compared and committed once, and it has one change event. This is the fast way to pass a
struct from one module to another (see the `structs` and `verilated` suites in [Benchmarks.md](Benchmarks.md)),
and what a struct port of a verilated module is by default ([VerilatedPorts.md](VerilatedPorts.md)).

dspsim has a second construct for a struct, the `PortStruct`: a group of ports with a signal per field.
The two are for different things:

| | a struct as a value (`Input<Bus>`) | a `PortStruct` |
|---|---|---|
| signals | one for the struct | one per field |
| fields | all in the direction of the port | each with a direction of its own, like the modports of a SystemVerilog interface |
| a write | replaces the whole value | writes one field |
| drivers | one process for the struct (or field signals, below) | a process per field |
| events | one change event | `change()`, `pos()`, `neg()` per field |
| cost of a change | one compare and one commit | one per field |

## In C++

```cpp
struct Iq
{
    int16_t re = 0;
    int16_t im = 0;
    bool operator==(const Iq &) const = default;
};

struct Bus
{
    uint32_t data = 0;
    uint8_t tag = 0;
    Iq iq;
    std::array<uint8_t, 3> taps{};
    bool operator==(const Bus &) const = default;
};

DSPSIM_MODULE(Stage)
{
public:
    dspsim::Input<uint8_t> clk{"clk"};
    dspsim::Input<Bus> s{"s"};
    dspsim::Output<Bus> m{"m"};

    DSPSIM_CTOR(Stage)
    {
        DSPSIM_METHOD(eval)->always(clk.pos());
    }

    void eval()
    {
        Bus bus = s.read();          // read() returns a const reference
        bus.tag += 1;
        m.write(bus);
    }
};

auto link = dspsim::Signal<Bus>::create("link");
a.m.bind(*link);
b.s.bind(*link);
```

`Signal<T>`, `Input<T>`, `Output<T>` and the arrays (`SignalArray<T>`, `InputArray<T>`, `OutputArray<T>`)
work for such a type like for a number. What differs:

- Only the fields are compared (the `operator==` of the struct), so the padding that the compiler puts
  between fields is not part of the value.
- The signal has no edges: `pos()` and `neg()` never occur. It has no width in bits and cannot be sliced.
- The member functions of the templates are compiled with the model. For the numbers (`uint8_t` ...
  `int64_t`, `float`, `double`) they are compiled once, in the core library.

The value does not have to be a struct: a `std::array` (a frame of samples as one signal) or an
enumeration works the same way.

## Describing a struct: `DSPSIM_STRUCT`

C++ code uses the struct directly. Code that does not know the type needs a description of it: Python, to
read and write the value, and `SignalBase::field(name)`. `DSPSIM_STRUCT` lists the fields:

```cpp
DSPSIM_STRUCT(Iq, re, im)
DSPSIM_STRUCT(Bus, data, tag, iq, taps)
```

- The macro goes after the struct, in the namespace of the struct. It does not change the struct: it defines
  a function, `dspsim_struct_layout(const Bus *)`, that returns the name, offset and type of each field (a
  `StructLayout`, [value_type.h](../src/include/dspsim/value_type.h)). The offsets and types are taken from the
  struct by the compiler, padding included.
- A field is a number, a `bool`, an enumeration (described as its underlying type), a struct with a
  `DSPSIM_STRUCT` of its own, or a `std::array` of those, nested for more dimensions. Any other type is a
  compile error. The struct must be trivially copyable, because its values are copied as bytes, and not `final`.
- A field that is not listed is still part of the value, but Python cannot see it.
- A struct without a `DSPSIM_STRUCT` works in C++, and its ports can be bound from Python. Reading or writing
  it from Python raises `TypeError`.

The structs of verilated modules are generated with their `DSPSIM_STRUCT`.

## In Python

A signal or a port whose value is a struct is a `RecordSignal` or a `RecordPort`, whatever the struct is.
Its fields are attributes, each a signal of its own that is read and written like any other signal:

```python
dut = Stage("dut")
s = dut.s.signal("s")  # the port creates the signal that it binds to
m = dut.m.signal("m")
dut.s.bind(s)
dut.m.bind(m)
context.elaborate()

s.data.d = 7  # write one field: the others keep their value
s.tag.d = 1
s.iq.re.d = 3  # a field of a nested struct
s.taps.d = [1, 2, 3]  # an array, or one element: s.taps[0].d = 1
context.run(10)

m.data.q  # read one field
m.iq.re.q
m.taps.q  # numpy array
await self.wait(m.tag.change())  # wait for one field
monitor.tag.bind(m.tag)  # bind a port of the field's type
dut.m.data.q  # the fields of a port are those of its signal
```

- A field is the signal of that field (the next section): a `FieldSignalU8` ... `FieldSignalFloat` for a
  number, which is a `SignalU8` ... with `q`, `d`, `value`, `read()`, `write()` and the events; a
  `RecordSignal` for a nested struct; an array of signals for an array. It is created when it is first
  asked for, at any time, also while the simulation runs, and it is the same object after that.
- Writes to several fields in one evaluation commit together, as one change of the struct.
- **Names.** A field comes before the attributes that every signal has, so `bus.id`, `bus.width` or
  `bus.kind` are the fields of those names. The exceptions are the names that a struct signal is used
  with: `read`, `write`, `value`, `q`, `d`, `dtype`, `fields`, `change`, `name`, `hier_name` (and for a port
  `bind`, `signal`, `is_output`). A field with such a name is an item, `bus["value"]`, as every field is.
- The fields of a port are those of the signal that it is bound to, so the port has them once it is bound.

The whole value is read and written as a numpy record:

```python
s.dtype  # the numpy dtype of the value, with the offsets of the C++ struct
s.fields  # ["data", "tag", "iq", "taps"]
s.write((7, 1, (3, -4), [1, 2, 3]))  # a tuple with the fields in order
s.write(
    {"data": 7, "tag": 1, "iq": {"re": 3, "im": -4}, "taps": [1, 2, 3]}
)  # or by name
context.run(10)

value = m.read()  # a Record: a copy of the value; also m.q and m.value
value.data, value.iq.re  # fields are attributes ...
value["data"]  # ... and items
value.tag = 5  # change the copy and write it back
s.write(value)
```

- A `dspsim.Record` is a `numpy.record` whose fields come first as attributes. In a plain `numpy.record`,
  `value.data` is numpy's buffer and `value.size`, `value.real` or `value.shape` are numpy's too, whatever
  the fields are called; in a `Record` they are the fields. Only `dtype`, `getfield` and `setfield` are
  never fields as attributes (use `value["dtype"]`). A nested struct is a `Record` and an array a numpy
  array, both views of the value.
- `write()` takes a `Record`, a tuple with the fields in order, a mapping with every field by name (a
  missing or unknown name is a `ValueError`), or anything else that numpy converts to the dtype.
- `port.bind(signal)` checks that the signal has the value type of the port, and raises `ValueError`
  otherwise. `port.signal(name)` works for every port.

A signal or port of another type without a class of its own is a `SignalBase` or a `PortBase`, with the
same `read()`, `write()`, `q`, `d` and `dtype`: an array as its value (`std::array<int16_t, 4>`) is a numpy
array, and a `float` or a `bool` a Python number.

An array of such ports or signals (`InputArray<Bus>`) is a `PortBaseArrayView` or a `SignalBaseArrayView`:
indexed and sliced like the other arrays, bound as a whole (`ports.bind(ports.signals("name"))`), and read
and written as one numpy array with the shape of the array:

```python
buses[1, 2].data.d = 5  # an element is a RecordSignal
values = (
    buses.read()
)  # numpy array of shape buses.shape; values["data"] is one field of all
values[1, 2].iq.re  # an element of the array is a Record
buses.write(values)  # an array, nested lists, or one value for every element
```

Python modules cannot declare a port with a struct as its value: there is no C++ type behind it. They
declare a port per field and bind it to the field of the struct signal (`self.tag.bind(bus.tag)`).

## The fields of a struct signal

A field of a struct signal is a signal of its own, in the way a slice of the bits of a signal is
([BitSlicing.md](BitSlicing.md)). In Python it is the attribute of that name; in C++:

```cpp
dspsim::Signal<Bus> bus{"bus"};
monitor.tag.bind(bus.field(&Bus::tag));                 // an Input<uint8_t> reads one field
driver.re.bind(bus.field(&Bus::iq).field(&Iq::re));     // a field of a field
tap.d.bind(bus.field(&Bus::taps).element(1));           // an element of an array
dspsim::SignalBase &data = bus.field("data");           // by name, with the DSPSIM_STRUCT
```

- The field signal (`FieldSignal<F>`, [field_signal.h](../src/include/dspsim/field_signal.h)) has the value of
  the field in the delta cycle in which the struct changes. Its events only occur when the field changes.
- What is written to it changes that field of the pending value of the struct and leaves the others. Writes
  to several fields and to the whole struct in one evaluation phase combine: the last write of a field wins.
- This is how a module with a struct as the value of a port connects to modules with a port per field, such
  as Python modules. It costs nothing for the fields that nobody asked for.
- A field signal can be created at any time, unlike other signals, which belong to the construction of a
  design: nothing has to find it when the design is elaborated. A field starts with the value that the
  struct has. `RecordSignalBase::field(name)` returns the same signal when it is asked again;
  `Signal<T>::field(&T::member)` creates one with each call.
- By name, a field needs the `DSPSIM_STRUCT` of the struct. `Signal<T>::field(&T::member)` does not.
- A signal with a struct as its value is a `RecordSignalBase` (and a port a `RecordPortBase`): the base
  class of `Signal<T>` for a class type `T` other than a `std::array`, which has the fields by name.

## How it works

- `Signal<T>::write()` compares the new value with the committed one and, if they differ, schedules the
  signal with `SignalBase::request_update()`. That function is in the core library, so that a signal of a
  model's type, which is compiled with the model, needs nothing of the scheduler.
- `SignalBase` and `PortBase` have a few virtual functions for a value without its type: `value_type()`
  (the description, or `nullptr`), `value_typeid()`, `value_data()`, `pending_data()` and `write_data()`
  (the value as bytes), `record()`, and `PortBase::create_signal()` and `bind_any()`. They are not on the
  run cycle.
- In Python, `RecordSignal` and `RecordPort` have a `__getattribute__` that looks for a field first. They
  are classes of their own so that the signals of numbers (`SignalU8`, ...) keep the plain attribute access.
- The Python side of a value is in `dspsim/_values.py`: the bindings (`value_bindings.h`) pass the
  description as nested tuples and the value as bytes, and numpy does the rest.
- A field signal is a `DerivedSignalBase`, like the signal of a bit selection: the source schedules it when
  it changes, and it is recomputed in the same update phase. `Signal<T>::write()` checks one pointer,
  `derived_`, to pass a write to a derived signal on to its source. The pointer is the last member of
  `SignalBase`, next to the value, so that a write that does not change the value touches no other cache line.
