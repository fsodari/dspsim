"""The library build: dspsim_add_module() builds the models of a pyproject.toml into one extension module."""

import gc
import importlib.util
from pathlib import Path

import numpy as np

import dspsim
from dspsim.builder import build_module
from dspsim.cpp_parser import supports_template_members
from dspsim.framework import (
    Clock,
    Context,
    InputS8,
    OutputS64,
    PortStruct,
    RecordPort,
    SignalS32,
)
from dspsim.project import ProjectConfig

HDL_DIR = Path(__file__).parent.parent / "hdl"
CPP_DIR = Path(__file__).parent / "cpp_modules"

PYPROJECT = f"""
[tool.dspsim]
name = "library"
sources = ["{HDL_DIR.as_posix()}/Skid.sv", "{HDL_DIR.as_posix()}/StructModel.sv"]
include_dirs = ["{HDL_DIR.as_posix()}"]

# Extra arguments for Verilator: for every model, and for one.
verilator_args = ["-DLIBRARY_BUILD"]

[tool.dspsim.parameters]
DW = 12

[tool.dspsim.models.StructModel]
verilator_args = ["--trace-structs"]

# The same module with a port per field of a struct, instead of a struct as the value of one port.
[tool.dspsim.models.FieldModel]
source = "{HDL_DIR.as_posix()}/StructModel.sv"
struct_ports = "fields"

# C++ modules. What they are built with is common to all of them.
[tool.dspsim.cpp]
include_dirs = ["{CPP_DIR.as_posix()}"]
sources = ["{CPP_DIR.as_posix()}/Mixer.cpp"]
definitions = {{ MIXER_GAIN = 3, MIXER_WIDE = true }}
bindings = "{CPP_DIR.as_posix()}/library_bindings.h"

[tool.dspsim.cpp.models.Counter]
header = "{CPP_DIR.as_posix()}/Counter.h"
cls = "demo::Counter"

# An instance of a class template.
[tool.dspsim.cpp.models.Delay8]
header = "{CPP_DIR.as_posix()}/Counter.h"
cls = "demo::Delay"
parameters = {{ T = "std::int8_t", MAX = 4 }}

# Defined in the source file.
[tool.dspsim.cpp.models.Mixer]
header = "{CPP_DIR.as_posix()}/Mixer.h"

# Uses classes that the bindings header binds.
[tool.dspsim.cpp.models.Hooked]
header = "{CPP_DIR.as_posix()}/Hooked.h"
methods = ["config", "configure", "mode", "set_mode", "apply"]
"""

CMAKELISTS = """
cmake_minimum_required(VERSION 4.0)
project(library_test LANGUAGES CXX)

find_package(dspsim REQUIRED)
dspsim_add_module(library ${CMAKE_SOURCE_DIR}/pyproject.toml library_test)
"""


def test_project_config(tmp_path):
    (tmp_path / "pyproject.toml").write_text(PYPROJECT)
    config = ProjectConfig.from_toml(
        tmp_path / "pyproject.toml", tmp_path / "generated"
    )

    assert list(config.models) == ["Skid", "StructModel", "FieldModel"]
    # The global parameters only apply to the models that have them.
    assert not config.models["StructModel"].parameters
    assert config.models["StructModel"].ports["u_in"].is_struct
    # A struct port is a value unless the model says otherwise.
    assert config.models["StructModel"].struct_ports == "value"
    assert config.models["FieldModel"].struct_ports == "fields"
    assert config.models["FieldModel"].source == config.models["StructModel"].source
    assert config.verilator_args == ["-DLIBRARY_BUILD"]
    assert config.models["Skid"].verilator_args == ["-DLIBRARY_BUILD"]
    assert config.models["StructModel"].verilator_args == [
        "-DLIBRARY_BUILD",
        "--trace-structs",
    ]
    assert list(config.cpp.models) == ["Counter", "Delay8", "Mixer", "Hooked"]

    delay = config.cpp.models["Delay8"]
    assert delay.cpp_type == "::demo::Delay<std::int8_t, 4>"
    assert {n: p.framework_type for n, p in delay.ports.items()} == {
        "clk": "InputU8",
        "d": "InputS8",
        "q": "OutputS8",
    }
    # The definitions apply to the parser: MIXER_WIDE makes the port 64 bits wide.
    mixer = config.cpp.models["Mixer"]
    assert mixer.ports["y"].framework_type == "OutputS64"
    assert "eval" not in [m.name for m in mixer.methods]
    # The classes of the bindings are named after the library, the model and a hash.
    assert delay.build_name.startswith("library_Delay8_")
    assert config.cpp.bindings == CPP_DIR / "library_bindings.h"
    assert CPP_DIR / "Mixer.cpp" in config.cpp.source_files


def test_library(tmp_path):
    """Build a library with a verilated model and C++ modules, and use it."""
    (tmp_path / "pyproject.toml").write_text(PYPROJECT)
    (tmp_path / "CMakeLists.txt").write_text(CMAKELISTS)
    build_dir = tmp_path / "build"
    build_module(tmp_path, build_dir, verbose=True)

    # The extension module: build/library.<abi>.so, or build/Release/library.pyd.
    (module_path,) = [
        path
        for pattern in ("**/library*.so", "**/library*.pyd")
        for path in build_dir.glob(pattern)
    ]
    # A library has its models under their own names, and a process has one class of a C++ class of a
    # name. The classes of a library that another test loaded (both have a Skid) must be gone first, and an
    # extension module is only freed by the garbage collector.
    gc.collect()
    spec = importlib.util.spec_from_file_location("library", module_path)
    assert spec is not None and spec.loader is not None
    library = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(library)
    dspsim.link_module(library)

    # What uses a class without Python bindings is left out and listed: (class, signature).
    assert sorted(library._unbound) == [
        ("Counter", "Counter(dspsim::ModuleName, const Opaque &)"),
        ("Counter", "configure(const Opaque &)"),
    ]

    assert library.Skid.DW == 12
    assert (library.Delay8.T, library.Delay8.MAX) == (np.int8, 4)

    with Context():
        skid = library.Skid("skid")
        assert skid.s_axis_tdata.width == 12

        # Struct ports, as in a JIT build: a struct is the value of one port.
        structs = library.StructModel("structs")
        assert type(structs.u_in) is RecordPort
        assert structs.u_in.dtype.names == (
            *("data", "valid", "taps", "iq", "wide", "flags"),
        )
        assert structs.w_out.dtype["wide"].shape == (4,)
        assert structs.p_in.signal("p_in").fields == ["data", "valid", "flags"]
        # Or a port per field, with struct_ports = "fields".
        fields = library.FieldModel("fields")
        assert isinstance(fields.u_in, PortStruct)
        assert fields.u_in.fields == ["data", "valid", "taps", "iq", "wide", "flags"]
        assert isinstance(fields.u_in.iq.re, InputS8)
        assert fields.w_out.wide.shape == (4,)
        assert fields.p_in.signals("p_in").flags.id.width == 3
        # The ports of one struct type are of one type in both: packed and unpacked structs of the library.
        structs.p_out.bind(structs.p_in.signal("p"))

        counter = library.Counter("counter", 5, limit=1000)
        assert (counter.step(), counter.limit()) == (5, 1000)

        delay = library.Delay8("delay")
        assert isinstance(delay.d, InputS8)
        if supports_template_members():
            assert library.Delay8("by_three", 3).delay() == 3

        mixer = library.Mixer("mixer", 10)
        assert (mixer.gain(), mixer.offset()) == (3, 10)
        assert isinstance(mixer.y, OutputS64)

        # The classes that the bindings header binds are in the library module.
        config = library.Config()
        config.gain = 3
        hooked = library.Hooked("hooked", config)
        assert hooked.mode() == library.Mode.Scale
        assert hooked.apply(5) == 15
        assert hooked.fill(3) == [0, 3, 6]
        # The header only adds fill() to Hooked.
        assert not hasattr(counter, "fill")

    # A simulation with a C++ module of the library.
    with Context() as context:
        clk = Clock("clk", 10)
        q = SignalS32("q")
        counter = library.Counter("counter", 5, limit=1000)
        counter.clk.bind(clk)
        counter.q.bind(q)

        context.elaborate()
        assert context.run_until(counter.wait_for(15)) is True
        count = q.q
        # The task completed on a clock edge: two more follow, 10 and 20 later.
        context.run(25)
        assert q.q == count + 10
