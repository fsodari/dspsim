# Ports of Verilated Modules

How the ports of a SystemVerilog module appear on the dspsim module that wraps it. A VHDL entity is turned
into SystemVerilog first ([Vhdl.md](Vhdl.md) has the VHDL types), so this is what its ports are too. The same rules apply to the
JIT builder (`vbuilder`, `build_vmodule`) and to library builds, because both read the ports from Verilator
(`verilator.py`) and generate the wrapper from the same templates.

| SystemVerilog port | dspsim port | Python class |
|---|---|---|
| up to 64 bits | `Input<T>` / `Output<T>`, `T` the smallest of 8, 16, 32, 64 bits that fits, signed if the port is | `InputU8` ... `OutputS64` |
| `real` | `Input<double>` / `Output<double>` | `InputFloat`, `OutputFloat` |
| unpacked array `logic [7:0] a [2][3]` | `InputArray<T>` / `OutputArray<T>` of that shape | `InputArrayU8` ... |
| more than 64 bits | array of 32-bit words | `InputArrayU32`, `OutputArrayU32` |
| struct, packed or unpacked | `PortStruct` with a port per field | `PortStruct` |
| array of structs | `PortStructArray`, whose elements are `PortStruct`s | `PortStructArray` |
| packed array, packed union | the vector of its bits: one of the first, second or fourth rows | |

An enumeration is its base type. A packed array (`logic [3:0][7:0]`, or of packed structs) is one vector, and
so is a packed union: its members are views of the same bits, which separate ports cannot be, because every
input port would write its own value to them. Bind the union to a signal and select the bits of a member
from the signal ([BitSlicing.md](BitSlicing.md)); writes to selections of one signal combine the way
writes to the members of a union do.

## Ports of more than 64 bits

dspsim has no integer type of more than 64 bits. A wider port is an array of 32-bit words with the least
significant word first, which is how Verilator stores it (`VlWide`). The words are the last dimension of the
array:

```systemverilog
input  logic [99:0]  a,            // InputArrayU32, shape (4,)
output logic [127:0] rows [3]      // OutputArrayU32, shape (3, 4)
```

- The most significant word has the bits that remain (4 for a 100-bit port: `a[3].width == 4`). An input is
  truncated to them when it is written to the model, so the bits above the port width do not reach it.
- The words are unsigned, also for a signed port.
- Every word is a signal and a port of its own. Words that are written in the same evaluation commit
  together, so the model is evaluated once.
- `pack()` puts element 0 of an array in the most significant bits, the opposite of the word order, and
  selections are limited to 64 bits ([BitSlicing.md](BitSlicing.md)): it is not the way to build a wide value.

In Python, `to_words()` and `from_words()` convert between an integer and the words, and `signal()` creates
the signal array for a width over 64:

```python
a = signal("a", width=100)  # SignalArrayU32 of shape (4,)
dut.a.bind(a)
a.d = to_words(value, len(a))  # negative values as two's complement
total = from_words(result.q)  # unsigned, all bits of the words
diff = from_words(result.q, width=100, signed=True)
rows.d = [to_words(row, 4) for row in values]
```

In C++ the port is declared with `DSPSIM_VINPUT_WIDE(name, width)` / `DSPSIM_VOUTPUT_WIDE(name, width)`
(`VInputWide<W>`, `VOutputWide<W>` in `vmodule/vport.h`) and binds to a `SignalArray<uint32_t>`.

## Struct ports

A struct port is a `PortStruct` (`dspsim/struct.h`): a group of ports with named fields. Every field of the
struct is a port of the kind that the table above gives for its type, and a field that is a struct is a
`PortStruct` again.

```systemverilog
typedef struct {
  logic signed [11:0] data;
  logic               valid;
  logic [5:0]         taps [4];
  iq_t                iq;          // another struct
} bus_t;

module Stage (input logic clk, input bus_t bus_in, output bus_t bus_out);
```

```python
stage = Stage("stage")
stage.bus_in.data  # InputS16, 12 bits, named "bus_in.data"
stage.bus_in.taps  # InputArrayU8, shape (4,)
stage.bus_in.iq.re  # a field of the nested struct
stage.bus_in.fields  # ["data", "valid", "taps", "iq"]
stage.bus_in["data"]  # the same port as stage.bus_in.data
```

An attribute of `PortStruct` itself (`name`, `fields`, `bind`, `signals`, `add`) hides a field of that name,
which is then reached as an item. In C++ the fields are members of the generated class (`stage.bus_in.data`),
and `input<T>(field)`, `output<T>(field)`, `input_array<T>(field)`, `output_array<T>(field)` and
`nested(field)` find them by name.

### Binding a struct as a whole

The fields bind like any other port (`stage.bus_in.data.bind(sig)`). `bind()` on the struct binds all of them,
to the fields of the same name in a `SignalStruct`, which is the same kind of group made of signals.
`signals(name)` creates one for a port struct, with a signal of the right type, width and shape for every
field:

```python
bus = a.bus_out.signals(
    "bus"
)  # signals "bus.data", "bus.valid", ...; bus.iq is a SignalStruct
a.bus_out.bind(bus)
b.bus_in.bind(bus)
bus.data.q
```

```cpp
SignalStruct bus = a.bus_out.signals("bus");
a.bus_out.bind(bus);
b.bus_in.bind(bus);
bus.signal<int16_t>("data").read();
bus.array<uint8_t>("taps");
bus.nested("iq").signal<int8_t>("re");
```

- Both sides must have the same fields, and each signal must have the type of its port. Otherwise `bind()`
  throws `std::invalid_argument` (`ValueError`) that lists every difference with the path of its field
  (`no field 'iq.im' in the signals`), and binds nothing.
- `bind(PortStruct &)` binds to the port struct of a parent module, field by field, like a port binds to a
  port. An input struct does not bind to an output struct.
- A `SignalStruct` can be made of existing signals: `SignalStruct("bus").add("data", data).add("iq", iq)`.
  A field is a signal, a signal array or a `SignalStruct`. In C++, a signal added by reference must outlive
  the struct; a `std::shared_ptr` or a Python object is kept alive by it. Copies of a `SignalStruct` refer to
  the same signals.
- All fields of a struct port have the direction of the port.

### Unpacked and packed structs

The two kinds look the same in dspsim and differ in how Verilator stores them, which is what the generated
class of a struct port (`verilator_struct.jinja`) deals with:

- **Unpacked** (`typedef struct { ... }`): a C++ struct with one member per field. A field port is an
  ordinary verilated port (`VInput`, `VOutputArray`, `VInputWide`, ...) on that member, so a field can be
  an unpacked array, wider than 64 bits, or `real`.
- **Packed** (`typedef struct packed { ... }`): one vector of bits with the first field in the most
  significant bits, in one integer up to 64 bits and in 32-bit words above. A field port reads and writes
  its own bits of the vector (`VInputBits`, `VOutputBits`), and the other fields keep theirs. A field of
  more than 64 bits is an array of words that can start at any bit (`VInputWideBits`, `VOutputWideBits`).
  The struct can be nested in a packed or an unpacked struct.
- **In an array**: each element is an object of the same generated class, constructed from one verilated
  struct of the array (`vl_element_pointers()`), or from a pointer to the first word of one packed vector
  (`vl_word_pointers()`).

In a trace, Verilator shows an unpacked struct field by field and a packed struct as one vector. Its
`--trace-structs` option splits packed structs (and packed arrays) into their fields, at the cost of larger
and slower traces. dspsim does not pass it; add it with `verilator_args` (below).

### Arrays of structs

An array of structs is a `PortStructArray`: an array whose elements are `PortStruct`s, indexed like the
other arrays. It is not the same thing as a struct of arrays, which is a `PortStruct` whose fields are
arrays; both exist, each for the SystemVerilog construct of that name.

```systemverilog
input bus_t buses [2][3]
```

```python
dut.buses.shape  # (2, 3)
dut.buses[1, 2]  # the PortStruct of one element, named "buses[1][2]"
dut.buses[1, 2].data  # InputS16, named "buses[1][2].data"
dut.buses[1]  # a part of the array: a PortStructArray of shape (3,)
for bus in dut.buses:
    ...  # the elements in row-major order
```

```cpp
dut.buses[{1, 2}].data.read();
```

The array binds as a whole to a `SignalStructArray`, an array of `SignalStruct`s, which `signals(name)`
creates. The shapes must be the same and every element must bind to its struct; otherwise `bind()` raises
with the differences and the places of the elements (`no field '[1][2].valid' in the signals`) and binds
nothing.

```python
buses = dut.buses.signals("buses")  # signals "buses[1][2].data", ...
dut.buses.bind(buses)
buses[1, 2].data.d = 5
```

**One field of every element.** A field name on the array gives that field of all elements as one array,
which is how a struct of arrays would have it. It is a view of the same ports or signals, so there is one
set of them and two ways to reach it:

```python
dut.buses.data  # InputArrayViewS16 of shape (2, 3); dut.buses.data[1, 2] is dut.buses[1, 2].data
dut.buses.taps.shape  # (2, 3, 4): the dimensions of a field that is an array come last
dut.buses.iq  # a nested struct of every element: a PortStructArray of shape (2, 3)
buses.data.d = values  # a whole field of the signals at once
buses.data.q  # numpy array of shape (2, 3)
```

```cpp
InputArrayView<int16_t> data = dut.buses.input_array<int16_t>("data");
PortStructArray iq = dut.buses.nested("iq");
SignalArrayView<int16_t> values = buses.array<int16_t>("data");
```

A struct can have an array of structs as a field (`bus_t buses [4]` inside another struct): the field is a
`PortStructArray`, and a `SignalStructArray` in the signals. Packed and unpacked structs both work as
elements. In C++, `PortStructArrayOf<S>` is the array that owns its structs of class `S`
(`PortStructArrayOf<BusIn> buses{"buses", {2, 3}};`); `PortStructArray` and its views refer to structs that
something else owns. In Python, `PortStructArray(name, shape, structs)` and
`SignalStructArray(name, shape, structs)` make an array of existing structs, in row-major order.

### Struct ports of hand-written modules

A struct port is a class derived from `PortStruct` whose ports are members, added in the constructor:

```cpp
struct BusIn : PortStruct
{
    Input<int16_t> data;
    Input<uint8_t> valid;

    BusIn(const std::string &name) : PortStruct(name), data(name + ".data", 12), valid(name + ".valid", 1)
    {
        add("data", data);
        add("valid", valid);
    }
};
```

The C++ module builder binds a public member of such a class as a struct port, and a `PortStructArrayOf`
member as an array of structs, like those of a verilated module ([CppBuilder.md](CppBuilder.md)). In a Python
module:

```python
class Stage(Module):
    def __init__(self, name: str):
        self.bus_in = PortStruct("bus_in")
        self.bus_in.add("data", InputS16("bus_in.data", 12)).add(
            "valid", InputU8("bus_in.valid", 1)
        )
```

## Annotations

The port annotations of a `vbuilder` class name the Python class of each port, and are checked against the
model: `a: InputArrayU32` for a wide port, `bus_in: PortStruct` for a struct port and `buses: PortStructArray`
for an array of structs (the fields are not annotated).

## Verilator arguments

`vbuilder` and `build_vmodule()` take extra arguments for Verilator as `verilator_args`, and a library takes
them as `verilator_args` in `[tool.dspsim]` (for every model) and in `[tool.dspsim.models.<Name>]` (for one,
after the common ones):

```python
@vbuilder(
    source=source, trace="fst", verilator_args=["--trace-structs", "-DSIMULATION"]
)
class Stage(VModule): ...
```

```toml
[tool.dspsim]
verilator_args = ["-DSIMULATION"]

[tool.dspsim.models.Stage]
verilator_args = ["--trace-structs"]
```

They are passed both when the module is read (so a definition that changes a port is seen) and when it is
verilated (`VERILATOR_ARGS` of the `verilate()` CMake function), after the arguments that dspsim passes
itself. They are part of what identifies a JIT build, so the same module with other arguments is a separate
build.

## Limits

- Unpacked unions are not supported: loading the module raises `ValueError` with the name of the port.
- The members of a packed union are not ports (see the table above).
- The names of the members of Verilator's C++ structs (`__PVT__<field>`) are internal to Verilator, like the
  `m_storage` of its arrays that `vport.h` reads. They are in one place, `Port.vl_member` in `module_info.py`.
