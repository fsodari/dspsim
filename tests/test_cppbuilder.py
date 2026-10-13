import os
import subprocess
import sys
import warnings
from itertools import pairwise
from pathlib import Path
from typing import Any

import numpy as np
import pytest

from dspsim.builder import CppBindingWarning, CppModule, build_cppmodule, cppbuilder
from dspsim.cpp_parser import (
    CppParseError,
    load_cpp_module_info,
    supports_template_members,
)
from dspsim.framework import (
    Clock,
    Context,
    InputArrayS16,
    InputArrayU32,
    InputFloat,
    InputS16,
    InputU8,
    InputU16,
    OutputArrayFloat,
    OutputArrayU32,
    OutputS8,
    OutputS32,
    OutputS64,
    PortStruct,
    PortStructArray,
    SignalArrayFloat,
    SignalArrayS16,
    SignalFloat,
    SignalS8,
    SignalS16,
    SignalS32,
    SignalS64,
    SignalU8,
    _Module,
    from_words,
    signal,
    to_words,
)

CPP_DIR = Path(__file__).parent / "cpp_modules"

# libclang 20 reads the constructors, methods and base classes of an instance of a class template. With an
# older one (the libclang wheel), a class template is bound with its ports and its ModuleName constructor.
TEMPLATE_MEMBERS = supports_template_members()


def test_cppbuilder():
    @cppbuilder(header=CPP_DIR / "Accumulator.h", verbose=True)
    class Accumulator(CppModule):
        clk: InputU8
        rst: InputU8
        d: InputS16
        q: OutputS32

    with Context("test_cppbuilder") as context:
        clk = Clock("clk", 10)
        rst = SignalU8("rst", 1)
        d = SignalS16("d")
        q = SignalS32("q")

        acc = Accumulator("acc")
        acc.clk.bind(clk)
        acc.rst.bind(rst)
        acc.d.bind(d)
        acc.q.bind(q)

        # The class derives from the C++ Module class of the framework.
        assert isinstance(acc, _Module)
        assert acc.__class__.__name__ == "Accumulator"
        assert acc.name == "acc"
        assert acc.context.name == "test_cppbuilder"
        assert isinstance(acc.clk, InputU8)
        assert isinstance(acc.d, InputS16)
        assert isinstance(acc.q, OutputS32)
        # The ports are named after the module they belong to.
        assert acc.d.hier_name == "root.acc.d"

        context.elaborate()

        context.run(100)
        assert q.q == 0

        rst.d = 0
        d.d = -3
        context.run(100)
        assert q.q == -30

        d.d = 1000
        context.run(50)
        assert q.q == 4970


def test_build_cppmodule():
    """A class in a namespace, with array ports, floating point ports and ports of a base class."""
    Scaler = build_cppmodule(
        "Scaler", CPP_DIR / "Scaler.h", cls="demo::Scaler", verbose=True
    )

    with Context() as context:
        clk = Clock("clk", 10)
        rst = SignalU8("rst", 0)
        x = SignalArrayS16("x", (4,))
        gain = SignalFloat("gain")
        y = SignalArrayFloat("y", (4,))

        scaler = Scaler("scaler")
        assert isinstance(scaler.x, InputArrayS16)
        assert isinstance(scaler.gain, InputFloat)
        assert isinstance(scaler.y, OutputArrayFloat)
        assert scaler.x.shape == (4,)

        scaler.clk.bind(clk)
        scaler.rst.bind(rst)
        scaler.x.bind(x)
        scaler.gain.bind(gain)
        scaler.y.bind(y)

        context.elaborate()

        gain.d = 0.5
        for i in range(4):
            x[i].d = i - 2
        context.run(20)

        assert [y[i].q for i in range(4)] == [-1.0, -0.5, 0.0, 0.5]

    # Building the same module again gives the class that is already imported.
    assert build_cppmodule("Scaler", CPP_DIR / "Scaler.h", cls="demo::Scaler") is Scaler


def test_cppbuilder_port_annotations():
    header = CPP_DIR / "Accumulator.h"

    with pytest.raises(TypeError, match="Type mismatch for port 'd'"):

        @cppbuilder(header=header, cls="Accumulator")
        class WrongType(CppModule):
            clk: InputU8
            rst: InputU8
            d: InputU16
            q: OutputS32

    with pytest.raises(TypeError, match="Partial port annotations"):

        @cppbuilder(header=header, cls="Accumulator")
        class Partial(CppModule):
            clk: InputU8
            rst: InputU8

    with pytest.raises(TypeError, match="Port 'enable' is not defined"):

        @cppbuilder(header=header, cls="Accumulator")
        class ExtraPort(CppModule):
            clk: InputU8
            rst: InputU8
            enable: InputU8
            d: InputS16
            q: OutputS32


def test_cpp_module_info():
    header = CPP_DIR / "Scaler.h"

    # The class is found with or without its namespace. Ports of base classes come first.
    for cls in ("Scaler", "demo::Scaler", "::demo::Scaler"):
        info = load_cpp_module_info(header, cls)
        assert info.cpp_name == "::demo::Scaler"
        assert {name: port.framework_type for name, port in info.ports.items()} == {
            "clk": "InputU8",
            "rst": "InputU8",
            "x": "InputArrayS16",
            "gain": "InputFloat",
            "y": "OutputArrayFloat",
        }

    with pytest.raises(LookupError, match="No class 'Missing'"):
        load_cpp_module_info(header, "Missing")
    with pytest.raises(TypeError, match="does not derive from dspsim::Module"):
        load_cpp_module_info(header, "NotAModule")
    with pytest.raises(TypeError, match="has no public constructor that can be bound"):
        load_cpp_module_info(CPP_DIR / "Counter.h", "Anonymous")
    with pytest.raises(TypeError, match="'T' of 'demo::Passthrough' has no default"):
        load_cpp_module_info(header, "Passthrough")


def test_cppbuilder_template():
    """Class annotations set the template parameters of a class template."""

    @cppbuilder(header=CPP_DIR / "Gain.h", verbose=True)
    class Gain(CppModule):
        # Template parameters. NEGATE and W keep their defaults.
        T: type = np.int16
        GAIN: int = 3
        SHIFT: int = 1
        NEGATE: bool
        W: type
        # Ports
        clk: InputU8
        d: InputS16
        q: OutputS64

    # The template arguments are attributes of the class.
    assert Gain.T is np.int16
    assert Gain.GAIN == 3
    assert Gain.SHIFT == 1
    assert Gain.NEGATE is False
    assert Gain.W is np.int64

    with Context() as context:
        clk = Clock("clk", 10)
        d = SignalS16("d")
        q = SignalS64("q")

        gain = Gain("gain")
        assert gain.GAIN == 3
        gain.clk.bind(clk)
        gain.d.bind(d)
        gain.q.bind(q)

        context.elaborate()

        d.d = 10
        context.run(20)
        assert q.q == 15

        d.d = -7
        context.run(20)
        assert q.q == -11


def test_build_cppmodule_template():
    """Other arguments of the same template. GAIN and SHIFT, before NEGATE, get their defaults."""
    Negate = build_cppmodule(
        "Negate",
        CPP_DIR / "Gain.h",
        cls="demo::Gain",
        parameters={"T": "std::int8_t", "NEGATE": True, "W": np.int8},
    )
    assert (Negate.T, Negate.GAIN, Negate.SHIFT, Negate.NEGATE, Negate.W) == (
        np.int8,
        1,
        0,
        True,
        np.int8,
    )

    with Context() as context:
        clk = Clock("clk", 10)
        d = SignalS8("d")
        q = SignalS8("q")

        negate = Negate("negate")
        assert isinstance(negate.q, OutputS8)
        negate.clk.bind(clk)
        negate.d.bind(d)
        negate.q.bind(q)

        context.elaborate()

        d.d = 5
        context.run(20)
        assert q.q == -5


def test_cppbuilder_template_annotations():
    header = CPP_DIR / "Gain.h"

    with pytest.raises(TypeError, match="Type mismatch for parameter 'GAIN'"):

        @cppbuilder(header=header, cls="Gain")
        class WrongAnnotation(CppModule):
            T: type = np.int16
            GAIN: str = "3"

    with pytest.raises(TypeError, match="Type mismatch for parameter 'GAIN'"):

        @cppbuilder(header=header, cls="Gain")
        class WrongValue(CppModule):
            T: type = np.int16
            GAIN: int = 1.5  # type: ignore

    with pytest.raises(TypeError, match="'T' of 'demo::Gain' has no default"):

        @cppbuilder(header=header, cls="Gain")
        class MissingType(CppModule):
            GAIN: int = 3

    # The ports depend on the template arguments.
    with pytest.raises(TypeError, match="Type mismatch for port 'd'"):

        @cppbuilder(header=header, cls="Gain")
        class WrongPort(CppModule):
            T: type = np.int32
            clk: InputU8
            d: InputS16
            q: OutputS64


def test_cpp_template_module_info():
    header = CPP_DIR / "Gain.h"

    def arguments(info):
        return {name: p.value for name, p in info.parameters.items()}

    def ports(info):
        return {name: port.framework_type for name, port in info.ports.items()}

    info = load_cpp_module_info(header, "Gain", parameters={"T": np.uint8, "W": float})
    assert info.cpp_name == "::demo::Gain"
    assert arguments(info) == {
        "T": "unsigned char",
        "GAIN": 1,
        "SHIFT": 0,
        "NEGATE": False,
        "W": "double",
    }
    assert [p.numpy_type for p in info.type_parameters] == ["uint8", "float64"]
    assert ports(info) == {"clk": "InputU8", "d": "InputU8", "q": "OutputFloat"}

    # A default argument can depend on the parameters before it, also when a later parameter is set.
    info = load_cpp_module_info(
        header, "Resize", parameters={"T": np.uint16, "REGISTERED": True}
    )
    assert arguments(info) == {
        "T": "unsigned short",
        "WIDTH": 16,
        "Q": "unsigned short",
        "REGISTERED": True,
    }
    assert ports(info) == {"d": "InputU16", "q": "OutputU16"}

    # A type that numpy does not have is given by its C++ name.
    info = load_cpp_module_info(header, "Stream", parameters={"T": "demo::Sample"})
    assert arguments(info) == {"T": "demo::Sample"}
    assert info.type_parameters[0].numpy_type is None
    assert ports(info) == {"tdata": None, "tvalid": "InputU8"}

    # A class that derives from an instance of a class template has the ports of that instance.
    info = load_cpp_module_info(header, "ByteSink")
    assert info.parameters == {}
    assert ports(info) == {
        "tdata": "InputU8",
        "tvalid": "InputU8",
        "tready": "OutputU8",
    }

    with pytest.raises(TypeError, match="has no template parameter 'BOGUS'"):
        load_cpp_module_info(header, "Gain", parameters={"T": np.int8, "BOGUS": 1})
    with pytest.raises(TypeError, match="has no template parameter 'T'"):
        load_cpp_module_info(header, "ByteSink", parameters={"T": np.int8})
    with pytest.raises(TypeError, match="neither the name of a C\\+\\+ type nor"):
        load_cpp_module_info(header, "Gain", parameters={"T": np.complex64})
    with pytest.raises(CppParseError, match="NoSuchType"):
        load_cpp_module_info(header, "Gain", parameters={"T": "NoSuchType"})

    # A base class that depends on a template parameter.
    if TEMPLATE_MEMBERS:
        info = load_cpp_module_info(header, "Sink", parameters={"T": np.uint16})
        assert ports(info) == {
            "tdata": "InputU16",
            "tvalid": "InputU8",
            "tready": "OutputU8",
        }
    else:
        with pytest.raises(NotImplementedError, match="needs libclang 20"):
            load_cpp_module_info(header, "Sink", parameters={"T": np.uint16})


def test_cppbuilder_methods():
    """Constructors with arguments and public methods."""
    with pytest.warns(CppBindingWarning) as record:
        Counter = build_cppmodule("Counter", CPP_DIR / "Counter.h", verbose=True)

    # What cannot be bound is left out with a warning that says why.
    not_bound = "C++ module Counter: '{}' is not bound: {}"
    record = [w for w in record if w.category is CppBindingWarning]
    assert sorted(str(w.message) for w in record) == [
        not_bound.format(
            "Counter(dspsim::ModuleName, const Opaque &)",
            "no Python bindings for demo::Opaque",
        ),
        not_bound.format(
            "configure(const Opaque &)", "no Python bindings for demo::Opaque"
        ),
        not_bound.format("fill(int *, int)", "'int *' is a pointer to a value"),
        not_bound.format(
            "sum_later(const std::vector<int> &)",
            "a coroutine that takes 'const std::vector<int> &' would refer to a value "
            "that is gone when it resumes: take the argument by value",
        ),
    ]
    # The warning points at the code that builds the module.
    assert record[0].filename == __file__
    for name in ("configure", "fill", "sum_later"):
        assert not hasattr(Counter, name)
    # Processes, function templates, operators and non-public methods are not methods for Python.
    for name in ("eval", "scaled", "__call__", "hidden"):
        assert name not in vars(Counter)

    assert Counter.version() == 2

    with Context() as context:
        clk = Clock("clk", 10)
        q = SignalS32("q")

        # Constructors, with positional and keyword arguments and with a default.
        default = Counter("default")
        assert (default.step(), default.limit()) == (1, 100)
        by_three = Counter("by_three", step=3)
        assert (by_three.step(), by_three.limit()) == (3, 100)
        counter = Counter("counter", 5, limit=22)
        assert (counter.step(), counter.limit()) == (5, 22)
        with pytest.raises(TypeError):
            Counter("wrong", "five")

        counter.clk.bind(clk)
        counter.q.bind(q)
        for other in (default, by_three):
            other.clk.bind(clk)
            other.q.bind(SignalS32(f"{other.name}_q"))

        # Methods of the class and of its base class. Counter::step() hides Labeled::step().
        assert counter.label() == "none"
        counter.set_label("five")
        assert counter.label() == "five"
        counter.set_step(4)
        assert counter.step() == 4

        # Literal defaults are Python defaults.
        assert counter.multiples() == [4, 8, 12]
        assert counter.multiples(n=2) == [4, 8]
        assert counter.remaining() == 22
        assert counter.remaining(enabled=False) is None
        assert counter.describe() == ("counter:five", 0)
        assert counter.describe("c") == ("c:five", 0)

        # Overloads.
        counter.load(7)
        assert counter.remaining() == 15
        counter.load([1, 2, 3])
        assert counter.remaining() == 16

        # A default that is not a literal comes from the C++ function.
        counter.reset(10)
        assert counter.remaining() == 12
        counter.reset()
        assert (counter.remaining(), counter.step()) == (22, 4)
        counter.reset(2, False)
        assert (counter.remaining(), counter.step()) == (20, 1)

        # The module itself, as a result and as an argument.
        assert counter.self() is counter
        default.load(9)
        counter.copy_from(default)
        assert counter.remaining() == 13

        # Classes of the framework, as a result (by reference) and as an argument.
        assert counter.output() is counter.q
        assert counter.read_from(q) == 0

        context.elaborate()
        counter.reset()
        counter.set_step(5)
        context.run(30)
        assert q.q == 15
        assert counter.read_from(q) == 15
        # 20, then 25 reaches the limit.
        context.run(20)
        assert q.q == 0

    # With strict, what cannot be bound is an error.
    with pytest.raises(TypeError, match=r"'fill\(int \*, int\)' is not bound"):
        build_cppmodule("Counter", CPP_DIR / "Counter.h", strict=True)


def test_cppbuilder_tasks():
    """Python awaits the methods that return a dspsim::Task."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", CppBindingWarning)
        Counter = build_cppmodule("Counter", CPP_DIR / "Counter.h")
    Ticker = build_cppmodule("Ticker", CPP_DIR / "Counter.h")

    with Context() as context:
        clk = Clock("clk", 10)
        q = SignalS32("q")

        counter = Counter("counter", 5, limit=1000)
        counter.clk.bind(clk)
        counter.q.bind(q)
        ticker = Ticker("ticker")
        ticker.clk.bind(clk)

        context.elaborate()

        # The framework binds the awaitable of a Task<bool>. No module binds it again.
        wait_for = counter.wait_for(15)
        assert type(wait_for).__name__ == "TaskBool"
        assert context.run_until(wait_for) is True
        assert context.run_until(counter.wait_cycles()) is None

        # A result type that the framework does not have is bound by the first module that needs it, and
        # the other module uses that.
        value_after = counter.value_after(2)
        assert type(value_after).__name__ == "_Task_int"
        assert type(ticker.ticks_until(0)) is type(value_after)
        first = context.run_until(value_after)
        assert context.run_until(counter.value_after(cycles=2)) == first + 10
        assert context.run_until(ticker.ticks_until(context.time + 35)) == 4

        record: Any = context.run_until(counter.record(4))
        assert [b - a for a, b in pairwise(record)] == [5, 5, 5]

        # Awaited from a Python coroutine.
        async def main():
            before = await counter.value_after(1)
            await counter.wait_cycles(cycles=3)
            return await counter.value_after(1) - before

        assert context.run_until(main()) == 20

        with pytest.raises(TimeoutError):
            context.run_until(counter.wait_for(7), timeout=100)


def test_cppbuilder_same_name(tmp_path, monkeypatch):
    """Classes of the same name are separate builds unless everything about them is the same."""
    # The headers are new files in every run: keep their builds out of the cache of the project.
    monkeypatch.setenv("DSPSIM_CACHE_DIR", str(tmp_path / "cache"))

    def write(directory: str, value: int, final: str = "") -> Path:
        (tmp_path / directory).mkdir(exist_ok=True)
        (tmp_path / directory / "twin_value.h").write_text(
            f"inline constexpr int twin_value = {value};\n"
        )
        header = tmp_path / directory / "Twin.h"
        header.write_text(
            "#pragma once\n"
            "#include <dspsim/module.h>\n"
            '#include "twin_value.h"\n'
            f"struct Twin {final}: public dspsim::Module\n"
            "{\n"
            "    Twin(dspsim::ModuleName) {}\n"
            "    int value() const { return twin_value; }\n"
            "};\n"
        )
        return header

    # Two classes called Twin, in different files. The second is final: it is bound as it is, while any
    # other class gets a C++ class of its own per build.
    first_header = write("first", 1)
    First = build_cppmodule("Twin", first_header)
    Second = build_cppmodule("Twin", write("second", 2, "final "))
    assert First is not Second
    assert (First.__name__, Second.__name__) == ("Twin", "Twin")

    # The same build again is the class that is already imported.
    assert build_cppmodule("Twin", first_header) is First

    # A file that the header includes has changed: a new build, next to the one that is loaded.
    write("first", 3)
    Third = build_cppmodule("Twin", first_header)
    assert Third is not First

    with Context():
        assert First("first").value() == 1
        assert Second("second").value() == 2
        assert Third("third").value() == 3


@pytest.mark.skipif(
    sys.platform == "win32", reason="CPLUS_INCLUDE_PATH is for GCC and Clang"
)
def test_cppbuilder_system_header(tmp_path):
    """
    A header of the system or of another library is not part of the build key, but the build system knows
    that the module depends on it: the module is rebuilt in place when the header changes.
    """
    # A directory that the compiler searches like /usr/include.
    system = tmp_path / "system"
    system.mkdir()
    library = system / "library.h"
    library.write_text("inline constexpr int library_value = 1;\n")
    header = tmp_path / "User.h"
    header.write_text(
        "#pragma once\n"
        "#include <dspsim/module.h>\n"
        "#include <library.h>\n"
        "struct User : public dspsim::Module\n"
        "{\n"
        "    User(dspsim::ModuleName) {}\n"
        "    int value() const { return library_value; }\n"
        "};\n"
    )
    script = (
        "from pathlib import Path\n"
        "from dspsim.builder import build_cppmodule\n"
        "from dspsim.framework import Context\n"
        f"User = build_cppmodule('User', Path({str(header)!r}))\n"
        "with Context():\n"
        "    print(User.__module__, User('user').value())\n"
    )
    environment = os.environ | {
        "DSPSIM_CACHE_DIR": str(tmp_path / "cache"),
        "CPLUS_INCLUDE_PATH": str(system),
    }

    def run() -> list[str]:
        # A module can only be loaded once in a process: every run is a new one.
        result = subprocess.run(
            [sys.executable, "-c", script],
            env=environment,
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.split()

    build, value = run()
    assert value == "1"
    assert run() == [build, "1"]

    # The same build, with what the library header has now.
    library.write_text("inline constexpr int library_value = 2;\n")
    assert run() == [build, "2"]


def test_cppbuilder_sources():
    """A class that is declared in a header and defined in a source file, with compile definitions."""
    header = CPP_DIR / "Mixer.h"
    sources = [CPP_DIR / "Mixer.cpp"]

    @cppbuilder(
        header=header,
        sources=sources,
        definitions={"MIXER_GAIN": 3, "MIXER_WIDE": None},
    )
    class Mixer(CppModule):
        clk: InputU8
        a: InputS16
        b: InputS16
        # MIXER_WIDE makes the output 64 bits wide: the parser sees the definitions.
        y: OutputS64

        # Methods can be declared for the type checker. The class that the builder returns has the real ones.
        def gain(self) -> int: ...
        def offset(self) -> int: ...
        def history(self) -> list[int]: ...

    # The process is registered in the source file, by a constructor of the class.
    assert "eval" not in vars(Mixer)

    with Context() as context:
        clk = Clock("clk", 10)
        a = SignalS16("a")
        b = SignalS16("b")
        y = SignalS64("y")

        mixer = Mixer("mixer", 10)
        assert (mixer.gain(), mixer.offset()) == (3, 10)
        assert Mixer("other").offset() == 0
        mixer.clk.bind(clk)
        mixer.a.bind(a)
        mixer.b.bind(b)
        mixer.y.bind(y)

        context.elaborate()
        a.d = 5
        b.d = 2
        context.run(20)
        assert y.q == 27
        assert mixer.history()[-1] == 27

    # Without MIXER_WIDE the port has another type.
    info = load_cpp_module_info(
        header, "Mixer", definitions={"MIXER_GAIN": 3}, sources=sources
    )
    assert info.ports["y"].framework_type == "OutputS32"
    assert [m.name for m in info.methods] == [
        "gain",
        "offset",
        "set_offset",
        "history",
    ]

    # The registration of eval belongs to the constructors of Mixer: Passive::eval() is a method.
    Passive = build_cppmodule(
        "Passive", header, sources=sources, definitions={"MIXER_GAIN": 3}
    )
    with Context():
        assert Passive("passive").eval() == 7


def test_cppbuilder_bindings():
    """A header with bindings of the user's own: classes that the module uses, and members for it."""
    header = CPP_DIR / "Hooked.h"

    # Without it, what uses the classes Config and Mode is not bound.
    info = load_cpp_module_info(header, "Hooked")
    assert [s.signature for s in info.skipped] == ["fill(int *, int)"]
    assert {c for m in info.methods for c in m.classes} == {
        "demo::Config",
        "demo::Mode",
    }

    # fill() is bound by the header in another form: leave the one that the builder cannot bind out.
    methods = ["config", "configure", "mode", "set_mode", "apply"]
    with warnings.catch_warnings():
        warnings.simplefilter("error", CppBindingWarning)
        Hooked = build_cppmodule(
            "Hooked",
            header,
            methods=methods,
            bindings=CPP_DIR / "hooked_bindings.h",
        )

    # The class is in a module of its own, which sys.modules has.
    assert vars(sys.modules[Hooked.__module__])["Hooked"] is Hooked

    with Context():
        config = Hooked.Config()
        config.gain = 3
        config.offset = 1

        # The constructor and the methods that use the classes are bound, because the header bound those.
        hooked = Hooked("hooked", config)
        assert (hooked.config().gain, hooked.config().offset) == (3, 1)
        assert hooked.mode() == Hooked.Mode.Scale
        assert hooked.apply(5) == 16

        hooked.set_mode(Hooked.Mode.Bypass)
        assert hooked.apply(5) == 5
        hooked.set_mode(Hooked.Mode.Scale)
        config.gain = 2
        hooked.configure(config)

        # The members that the header added.
        assert hooked.fill(3) == [1, 3, 5]
        assert hooked.apply_again(5) == 11


def test_cppbuilder_packages(tmp_path, monkeypatch):
    """A module that uses a library, found as a CMake package."""
    monkeypatch.setenv("DSPSIM_CACHE_DIR", str(tmp_path / "cache"))

    # A package with a header and a compile definition: gainlib::gainlib.
    prefix = tmp_path / "prefix"
    (prefix / "include" / "gainlib").mkdir(parents=True)
    (prefix / "include" / "gainlib" / "gain.h").write_text(
        "#pragma once\n"
        "namespace gainlib\n"
        "{\n"
        "    using value_t = short;\n"
        "    inline int apply(int value) { return value * GAINLIB_FACTOR; }\n"
        "}\n"
    )
    (prefix / "lib" / "cmake" / "gainlib").mkdir(parents=True)
    (prefix / "lib" / "cmake" / "gainlib" / "gainlib-config.cmake").write_text(
        "add_library(gainlib::gainlib INTERFACE IMPORTED)\n"
        "set_target_properties(gainlib::gainlib PROPERTIES\n"
        '    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"\n'
        '    INTERFACE_COMPILE_DEFINITIONS "GAINLIB_FACTOR=7")\n'
    )
    monkeypatch.setenv("CMAKE_PREFIX_PATH", str(prefix))

    header = tmp_path / "Amplifier.h"
    header.write_text(
        "#pragma once\n"
        "#include <dspsim/module.h>\n"
        "#include <dspsim/port.h>\n"
        "#include <gainlib/gain.h>\n"
        "struct Amplifier : public dspsim::Module\n"
        "{\n"
        '    dspsim::Input<gainlib::value_t> d{"d"};\n'
        "    Amplifier(dspsim::ModuleName) {}\n"
        "    int amplify(int value) const { return gainlib::apply(value); }\n"
        "};\n"
    )

    # The parser reads the header with the include directories of the package, which CMake knows.
    Amplifier = build_cppmodule(
        "Amplifier", header, packages=["gainlib"], libraries=["gainlib::gainlib"]
    )
    with Context():
        amplifier = Amplifier("amplifier")
        assert isinstance(amplifier.d, InputS16)
        assert amplifier.amplify(6) == 42

    with pytest.raises(RuntimeError, match="could not configure the packages"):
        build_cppmodule("Amplifier", header, packages=["no_such_package"])


def test_cpp_function_info():
    header = CPP_DIR / "Counter.h"
    info = load_cpp_module_info(header, "Counter")

    assert [c.signature for c in info.constructors] == [
        "Counter(dspsim::ModuleName)",
        "Counter(dspsim::ModuleName, int, int)",
        "Counter(dspsim::ModuleName, const Opaque &)",
    ]
    # The types of the arguments after the name, and the default that Python sees.
    (by_step,) = info.constructors[1].overloads
    assert by_step.types == ["int", "int"]
    assert by_step.nb_args == ['nanobind::arg("step")', 'nanobind::arg("limit") = 100']
    # A class that may have no Python bindings is checked when the module is imported.
    assert info.constructors[2].classes == ["demo::Opaque"]

    methods = {m.signature: m for m in info.methods}
    assert list(methods) == [
        "step()",
        "set_step(int)",
        "limit()",
        "load(int)",
        "load(const std::vector<int> &)",
        "multiples(std::size_t)",
        "reset(int, bool)",
        "remaining(bool)",
        "describe(const std::string &)",
        "version()",
        "output()",
        "read_from(const dspsim::Signal<int32_t> &)",
        "self()",
        "copy_from(const Counter &)",
        "wait_cycles(int)",
        "wait_for(int)",
        "value_after(int)",
        "record(std::size_t)",
        "configure(const Opaque &)",
        "label()",
        "set_label(const std::string &)",
    ]
    assert [s.signature for s in info.skipped] == [
        "fill(int *, int)",
        "sum_later(const std::vector<int> &)",
    ]
    # The result types of the tasks that Python awaits.
    assert {m.name: m.task_type for m in info.methods if m.task_type} == {
        "wait_cycles": "void",
        "wait_for": "bool",
        "value_after": "int",
        "record": "std::vector<int>",
    }
    assert methods["record(std::size_t)"].task_name == "_Task_std_vector_int"
    assert info.headers == ["optional.h", "pair.h", "string.h", "vector.h"]

    # One binding with Python defaults if every default is a literal.
    (describe,) = methods["describe(const std::string &)"].overloads
    assert describe.nb_args == ['nanobind::arg("prefix") = "counter"']
    # Else one binding per number of arguments, without defaults.
    assert [o.types for o in methods["reset(int, bool)"].overloads] == [
        ["int", "bool"],
        ["int"],
        [],
    ]
    assert methods["version()"].is_static
    # A reference to an object is returned as a reference.
    assert (
        methods["output()"].return_policy == "nanobind::rv_policy::reference_internal"
    )
    # The ports and signals of the numeric types are instantiated explicitly (port.h, signal.h), and
    # libclang spells such a class the way that declaration does.
    assert methods["output()"].classes == ["dspsim::Output<int32_t>"]
    assert methods["read_from(const dspsim::Signal<int32_t> &)"].classes == [
        "dspsim::Signal<int32_t>"
    ]

    # A reference to the module itself is a reference to the class of the bindings, M, which is what
    # Python has. No other class needs Python bindings for it.
    (copy_from,) = methods["copy_from(const Counter &)"].overloads
    assert copy_from.types == ["const M &"]
    assert methods["copy_from(const Counter &)"].classes == []
    assert methods["self()"].classes == []
    assert methods["self()"].return_policy == "nanobind::rv_policy::reference_internal"

    # The methods to bind can be selected.
    info = load_cpp_module_info(header, "Counter", methods=["load", "label"])
    assert [m.signature for m in info.methods] == [
        "load(int)",
        "load(const std::vector<int> &)",
        "label()",
    ]
    assert info.skipped == []
    info = load_cpp_module_info(header, "Counter", methods=False)
    assert info.methods == [] and info.skipped == []
    assert len(info.constructors) == 3
    with pytest.raises(LookupError, match="has no public method 'missing'"):
        load_cpp_module_info(header, "Counter", methods=["load", "missing"])

    # A constructor that needs an argument besides the name.
    info = load_cpp_module_info(CPP_DIR / "Scaler.h", "NeedsArgument")
    assert [c.signature for c in info.constructors] == [
        "NeedsArgument(dspsim::ModuleName, int)"
    ]


def test_cppbuilder_template_methods():
    """The constructors and methods of a class template need libclang 20."""
    header = CPP_DIR / "Counter.h"
    parameters = {"T": np.int16, "MAX": 4}

    if TEMPLATE_MEMBERS:
        with warnings.catch_warnings():
            warnings.simplefilter("error", CppBindingWarning)
            Delay = build_cppmodule("Delay", header, parameters=parameters)
    else:
        with pytest.warns(
            CppBindingWarning, match="need libclang 20 or newer"
        ) as record:
            Delay = build_cppmodule("Delay", header, parameters=parameters)
        # The other constructor and the three methods.
        assert len([w for w in record if w.category is CppBindingWarning]) == 4
        with pytest.raises(TypeError, match="need libclang 20 or newer"):
            build_cppmodule("Delay", header, parameters=parameters, strict=True)

    assert (Delay.T, Delay.MAX) == (np.int16, 4)

    with Context() as context:
        clk = Clock("clk", 10)
        d = SignalS16("d")
        q = SignalS16("q")

        # The constructor that only takes the name is always bound.
        delay = Delay("delay")
        delay.clk.bind(clk)
        delay.d.bind(d)
        delay.q.bind(q)

        if TEMPLATE_MEMBERS:
            by_three = Delay("by_three", 3)
            by_three.clk.bind(clk)
            by_three.d.bind(d)
            by_three.q.bind(SignalS16("by_three_q"))
            assert (delay.delay(), by_three.delay()) == (1, 3)
        else:
            assert not hasattr(delay, "delay")

        context.elaborate()
        for value in (1, 2, 3):
            d.d = value
            context.run(10)
        assert q.q == 3

        if TEMPLATE_MEMBERS:
            # The default of contents() is the template parameter MAX: the C++ function supplies it.
            assert delay.contents() == [3, 2, 1, 0]
            assert delay.contents(2) == [3, 2]
            delay.set_delay(3)
            context.run(10)
            assert delay.contents() == [3, 3, 2, 1]
            assert q.q == 2


def test_cpp_struct_port_info():
    """A public member of a class derived from PortStruct is a struct port."""
    info = load_cpp_module_info(CPP_DIR / "StructPorts.h", "demo::StructStage")
    assert {name: port.framework_type for name, port in info.ports.items()} == {
        "bus_in": "PortStruct",
        "bus_out": "PortStruct",
        # The class of this one is an instance of a class template.
        "sum": "PortStruct",
        "wide_in": "InputArrayU32",
        "wide_out": "OutputArrayU32",
        # Arrays of structs.
        "samples": "PortStructArray",
        "conjugates": "PortStructArray",
    }
    assert info.ports["bus_in"].is_struct
    assert info.ports["bus_in"].ctype == "demo::BusIn"
    assert not info.ports["wide_in"].is_struct


def test_cppbuilder_struct_ports():
    """The struct ports of a C++ module give their fields by name and bind as a whole."""

    @cppbuilder(header=CPP_DIR / "StructPorts.h", cls="demo::StructStage")
    class StructStage(CppModule):
        bus_in: PortStruct
        bus_out: PortStruct
        sum: PortStruct
        wide_in: InputArrayU32
        wide_out: OutputArrayU32
        samples: PortStructArray
        conjugates: PortStructArray

    with Context() as context:
        stage = StructStage("stage")
        # An array of structs: the elements are structs.
        assert isinstance(stage.samples, PortStructArray)
        assert stage.samples.shape == (3,)
        assert stage.samples[2].re.name == "samples[2].re"
        assert stage.samples.re.shape == (3,)
        assert isinstance(stage.bus_in, PortStruct)
        assert stage.bus_in.name == "bus_in"
        assert stage.bus_in.fields == ["data", "valid", "taps", "iq"]
        assert isinstance(stage.bus_in.data, InputS16)
        assert stage.bus_in.data.width == 12
        assert stage.bus_in.taps.shape == (4,)
        assert stage.bus_in.iq.fields == ["re", "im"]
        assert isinstance(stage.bus_out.iq.re, OutputS8)
        assert stage.sum.fields == ["re", "im"]

        bus_in = stage.bus_in.signals("bus_in")
        bus_out = stage.bus_out.signals("bus_out")
        total = stage.sum.signals("sum")
        wide_in, wide_out = signal("wide_in", width=100), signal("wide_out", width=100)
        stage.bus_in.bind(bus_in)
        stage.bus_out.bind(bus_out)
        stage.sum.bind(total)
        stage.wide_in.bind(wide_in)
        stage.wide_out.bind(wide_out)
        samples = stage.samples.signals("samples")
        conjugates = stage.conjugates.signals("conjugates")
        stage.samples.bind(samples)
        stage.conjugates.bind(conjugates)
        context.elaborate()

        samples.re.d = [1, 2, 3]
        samples[1].im.d = 7
        bus_in.data.d = -1234
        bus_in.valid.d = 1
        bus_in.taps.d = [1, 2, 3, 4]
        bus_in.iq.re.d = 20
        bus_in.iq.im.d = -3
        wide_in.d = to_words(0x5_00000000_FFFFFFFF_12345678, 4)
        context.run(1)

        assert bus_out.data.q == 1234
        assert bus_out.valid.q == 1
        assert list(bus_out.taps.q) == [4, 3, 2, 1]
        assert (bus_out.iq.re.q, bus_out.iq.im.q) == (-20, 3)
        assert (total.re.q, total.im.q) == (17, 23)
        assert from_words(wide_out.q) == 0xA_FFFFFFFF_00000000_EDCBA987
        assert list(conjugates.re.q) == [1, 2, 3]
        assert [c.im.q for c in conjugates] == [0, -7, 0]

    with pytest.raises(TypeError, match="Type mismatch for port 'bus_in'"):

        @cppbuilder(header=CPP_DIR / "StructPorts.h", cls="demo::StructStage")
        class WrongStruct(CppModule):
            bus_in: InputS16
            bus_out: PortStruct
            sum: PortStruct
            wide_in: InputArrayU32
            wide_out: OutputArrayU32
            samples: PortStructArray
            conjugates: PortStructArray
