# VHDL Models

A synthesizable VHDL entity is a model like a SystemVerilog module is: `vbuilder`, `build_vmodule()` and
`[tool.dspsim]` take a `.vhd` or `.vhdl` file as a source, and the result is the same kind of verilated
module, in Python and in C++.

```python
from dspsim.builder import VModule, vbuilder
from dspsim.framework import InputS16, InputU8, OutputS16, OutputU8


@vbuilder(source="hdl/Skid.vhd", trace="fst")
class Skid(VModule):
    DW: int = 14  # a generic of the entity
    clk: InputU8
    rst: InputU8
    s_axis_tdata: InputS16  # signed(DW-1 downto 0)
    s_axis_tvalid: InputU8
    s_axis_tready: OutputU8
    m_axis_tdata: OutputS16
    m_axis_tvalid: OutputU8
    m_axis_tready: InputU8
```

dspsim does not simulate VHDL itself. [GHDL](https://github.com/ghdl/ghdl) synthesizes the entity to a Verilog
netlist (`ghdl synth --out=verilog`), dspsim writes a SystemVerilog module around the netlist that has the
interface of the entity, and from there on everything is the Verilator flow: the model is compiled C++, with
no simulator behind it, and tracing works as for any verilated module.

## Requirements

- **GHDL**, version 5 or 6 (tested with 5.0.1 and 6.0.0, with the mcode and the LLVM JIT back ends). dspsim
  runs the executable that `DSPSIM_GHDL` names, or `ghdl` on the path. It is only needed to build a model: Python packages and
  libraries that were built from VHDL do not need it to run.
- **Synthesizable VHDL.** What `ghdl synth` accepts is what dspsim can model: no `wait for`, no files, no
  `real` signals. GHDL calls its synthesis experimental; it handles the VHDL-2008 that the tests here use
  (records, functions, loops, arrays of several dimensions, instances of other entities), and reports what
  it does not.

## Sources

`source` is the file that declares the entity, which by default is named like the file (`Skid.vhd` has the
entity `Skid`; `vhdl_entity="Other"` names another one). Every other VHDL file of the design, the packages
and the entities that it instantiates, goes in `vhdl_sources`, in any order: GHDL works out what the entity
needs.

```python
VhdlModel = build_vmodule(
    "VhdlModel",
    source="hdl/VhdlModel.vhd",
    include_dirs=[],
    parameters={"DW": 9, "MODE": "sub"},
    vhdl_sources=["hdl/vhdl_model_pkg.vhd", "hdl/Accumulator.vhd"],
)
```

| Argument | |
|---|---|
| `vhdl_sources` | The other VHDL files of the design |
| `vhdl_entity` | The entity, if it is not named like the source file |
| `vhdl_std` | The VHDL standard: `"08"` (default), `"93"`, `"19"`, ... |
| `ghdl_args` | Extra arguments for GHDL when it analyzes, elaborates and synthesizes: `["-fsynopsys"]`, `["-P/path/to/library"]` |

All files are analyzed into the library `work`. A design that needs other libraries can have them compiled
beforehand and found with `-P` in `ghdl_args`.

## Generics

The generics of the entity are the parameters of the model: class annotations with `vbuilder`,
`parameters=` with `build_vmodule()`, and class attributes of the result (`Skid.DW`), with the names as the
entity writes them.

| VHDL | Python | |
|---|---|---|
| `integer`, `natural`, `positive`, an integer range | `int` | |
| `boolean` | `int`, 0 or 1 | |
| `string` | `str` | |
| `real` | `float` | Read only: GHDL cannot set a `real` generic from outside. Setting one raises `GhdlError` |
| anything else (`std_logic_vector`, an enumeration, `time`) | | Keeps its default. Not a parameter |

The netlist is synthesized for one set of values, so every set of values is a model of its own, as it is
for a SystemVerilog module. Values that the design does not elaborate with raise `GhdlError` with GHDL's
message.

## Ports

Every port has the type that the VHDL type maps to. The rest of this table is in
[VerilatedPorts.md](VerilatedPorts.md), which describes the dspsim ports of these SystemVerilog types.

| VHDL | SystemVerilog in the wrapper | dspsim |
|---|---|---|
| `std_logic`, `std_ulogic`, `bit`, `boolean` | `logic` | `InputU8` / `OutputU8`, 1 bit |
| `std_logic_vector`, `unsigned`, `bit_vector`, any array of the above | `logic [N-1:0]` | unsigned, by width |
| `signed` | `logic signed [N-1:0]` | signed, by width |
| `integer range a to b` | the bits of the range, `signed` if `a` is negative | by width |
| an enumeration | `logic [N-1:0]`, the position of the value | unsigned, by width |
| an array of anything else | unpacked array, a dimension for each of the VHDL type | `InputArray*` / `OutputArray*` |
| a record | `struct` | `PortStruct` |
| an array of records | unpacked array of structs | `PortStructArray` |
| a vector of more than 64 bits | `logic [N-1:0]` | array of 32-bit words |

- **Modes**: `in` is an input; `out` and `buffer` are outputs. `inout` and `linkage` are not supported.
- **Array indices**: element `i` of a dspsim array is the `i`-th from the lowest index of the VHDL range,
  whatever the direction of the range. `v(0 to 3)` and `v(3 downto 0)` both have `v(2)` at index 2, and
  `v(1 to 4)` has `v(1)` at index 0.
- **Names**: generics and ports keep the spelling of the entity (`DW`, `s_axis_tdata`). The elements of
  records, and the names of the record types, are in lower case, as GHDL reports them. A name that is a
  keyword of SystemVerilog, C++ or Python (`int`, `byte`, `class`, `in`) gets an underscore at the end:
  `int_`.
- **Unconstrained ports**: the entity is elaborated as the top of a design, so the bounds of its ports
  must follow from its generics.
- **Types with the same name**: dspsim finds the type of a port by its name. A design with two different
  types of one name, in two packages, is rejected if a port uses that name.

## In a library

A VHDL file in `sources` is a model, like a SystemVerilog file:

```toml
[tool.dspsim]
name = "library"
sources = ["hdl/*.sv", "hdl/Skid.vhd", "hdl/VhdlModel.vhd"]
# The other VHDL files of the designs: packages, and the entities that the models instantiate.
vhdl_sources = ["hdl/vhdl_model_pkg.vhd", "hdl/Accumulator.vhd"]
vhdl_std = "08"
ghdl_args = []

[tool.dspsim.parameters]
DW = 10

[tool.dspsim.models.VhdlModel]
entity = "VhdlModel"            # if it is not named like the file
vhdl_sources = []               # more files, for this model
parameters = { N = 2, MODE = "sub" }
```

The SystemVerilog is written when CMake configures the library, under the directory of the generated files,
and CMake configures again when a VHDL file changes.

## In C++

`dspsim_vhdl_source()` (`cmake/dspsim-utils.cmake`) writes the SystemVerilog of an entity when CMake
configures and gives its path, for `verilate()`:

```cmake
dspsim_vhdl_source(SKID_SV
    SOURCE hdl/Skid.vhd
    SOURCES hdl/pkg.vhd          # the other VHDL files
    GENERICS DW=16)
verilate(my_models SOURCES ${SKID_SV} PREFIX VSkid)
```

`ENTITY`, `STD`, `GHDL_ARGS` and `OUTPUT_DIR` are optional. The module has the name of the entity, and the
C++ wrapper of the model is that of any verilated module (`DSPSIM_VMOD`, `DSPSIM_VINPUT_S`, ...:
`tests/cpp/SkidVhdl.h`). The function runs `python -m dspsim.ghdl`, which is also a command of its own:

```bash
python -m dspsim.ghdl hdl/Skid.vhd [other.vhd ...] --output-dir build/vhdl -g DW=16
# prints the path of the SystemVerilog file
```

## Traces

`open_trace()` works as for a SystemVerilog model. The trace has the entity, the netlist as the instance
`impl_` inside it, and the instances of the design below that:

- The ports, and the signals that the VHDL declares, have their names. A record port is a scope with its
  elements.
- The nets that GHDL makes up for expressions (`n123`) are left out.
- An enumeration shows as the position of its value, not as its name.

## How it works

`dspsim.ghdl.transpile()` writes one SystemVerilog file for an entity and a set of generic values:

1. `ghdl -i` imports the files and `ghdl -m` analyzes what the entity needs.
2. `ghdl -r <entity> -g... --dump-rti --disp-tree=port --stop-time=0ns` elaborates the design and prints
   its generics and ports with their elaborated types, bounds evaluated, and the type declarations of the
   packages. `parse_rti()` reads the entity from that. This is a debugging output of GHDL and not a
   documented interface: `tests/test_vhdl.py` has a sample of it, and the tests that run GHDL check the
   whole path.
3. `ghdl synth --out=verilog` writes the netlist, with the generics as constants.
4. `wrapper()` writes the module of the entity: the generics as parameters, the ports with their types,
   and an instance of the netlist. The netlist has lost the types: every port is an unsigned vector, a
   record port is one port per element, and arrays and the records inside are packed into one vector, the
   elements of a record from the least significant bit in the order they are declared, the elements of an
   array from the most significant bit, left to right. The wrapper concatenates the fields in that order,
   and refuses a netlist whose ports do not have the bits that the types have.

The file is `<cache>/vhdl/<entity>_<digest>/<Entity>.sv`, where the digest covers the contents of the VHDL
files, the generics, the options, the version of GHDL and the generator itself, so it is written once and
found again. That file is the source of the model for everything after: `ModuleInfo.source`, the build key
of a JIT build, `verilate()`.

## Limits

- Synthesizable VHDL only, as far as `ghdl synth` goes.
- One library, `work`, unless others are precompiled.
- `inout` ports, `real` ports, and generics of other types than integer, boolean, string and real (read
  only) are not supported.
