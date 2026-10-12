"""HDL Module information."""

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Literal

type ParamValueT = int | float | str

_INTEGRAL_KEYWORDS = ("bit", "logic", "int", "integer")


@dataclass
class Parameter:
    name: str
    keyword: str
    signed: bool
    width: int
    value: ParamValueT | None

    @property
    def ctype(self) -> str:
        if self.keyword in ["bit", "int", "logic"]:
            return "int"
        elif self.keyword in ["real"]:
            return "double"
        elif self.keyword in ["string"]:
            # return "const char*"
            return "std::string"
        else:
            raise ValueError(f"Unsupported keyword: {self.keyword}")

    @property
    def pytype(self) -> type:
        if self.keyword in ["bit", "int", "logic"]:
            return int
        elif self.keyword in ["real"]:
            return float
        elif self.keyword in ["string"]:
            return str
        else:
            raise ValueError(f"Unsupported keyword: {self.keyword}")

    @property
    def gparam_val(self) -> str:
        if self.keyword in ["string"]:
            return f'"{self.value}"'
        elif self.keyword in ["logic"]:
            if self.width == 32 or self.width == 64:
                return str(self.value)
            elif self.width % 4 == 0:
                return f"{self.width}'h{self.value:x}"
            else:
                # return binary representation
                return f"{self.width}'b{self.value:b}"
        else:
            return str(self.value)

    @property
    def cpp_val(self) -> str:
        if self.keyword in ["string"]:
            return f'"{self.value}"'
        else:
            return str(self.value)


@dataclass
class Port:
    name: str
    keyword: str
    signed: bool
    width: int
    direction: str
    shape: tuple[int, ...]

    @property
    def is_array(self) -> bool:
        """Unpacked array ports have a non-empty shape."""
        return len(self.shape) > 0

    @property
    def is_signed_int(self) -> bool:
        """Signed integral ports map to signed stdint types (InputS8, OutputS32, ...)."""
        return self.signed and self.keyword in _INTEGRAL_KEYWORDS

    @property
    def vmacro(self) -> str:
        """The dspsim vport macro used to declare this port on a generated VModule."""
        array = "_ARRAY" if self.is_array else ""
        sign = "_S" if self.is_signed_int else ""
        return f"DSPSIM_V{self.direction.upper()}{array}{sign}"

    @property
    def stdint_size(self) -> int:
        if self.width <= 8:
            return 8
        elif self.width <= 16:
            return 16
        elif self.width <= 32:
            return 32
        elif self.width <= 64:
            return 64
        else:
            raise ValueError(f"Unsupported width: {self.width}")


@dataclass
class ModuleInfo:
    # Name of the generated C++ class, which the generated files, targets and extension module are named after.
    name: str
    source: Path
    parameters: dict[str, Parameter]
    ports: dict[str, Port]
    trace: Literal["vcd", "fst"] | None = None
    # Name of the Python class, if it is not `name`: the JIT builder gives every build a name of its own.
    python_name: str | None = None
    # Every file that Verilator read for the module: the source and what it includes or instantiates.
    source_files: list[Path] = field(default_factory=list)


@dataclass
class CppPort:
    """A port member (Input<T>, Output<T>, InputArray<T> or OutputArray<T>) of a C++ module."""

    name: str
    direction: Literal["input", "output"]
    is_array: bool
    # Canonical C++ spelling of the element type, e.g. "short".
    ctype: str
    # Suffix of the framework classes for the element type ("U8", "S16", "Float", ...), or None if the
    # framework binds no port of that type.
    suffix: str | None

    @property
    def framework_type(self) -> str | None:
        """Name of the framework class that Python sees for this port, e.g. "InputArrayS16"."""
        if self.suffix is None:
            return None
        array = "Array" if self.is_array else ""
        return f"{self.direction.capitalize()}{array}{self.suffix}"


@dataclass
class CppTemplateParameter:
    """A template parameter of a C++ module that is a class template."""

    name: str
    # "type" for a type parameter, "int" and "bool" for a non-type parameter of an integer type or of bool.
    # "other" for anything else, which keeps its default argument.
    kind: Literal["type", "int", "bool", "other"]
    # Canonical C++ type of a non-type parameter, e.g. "unsigned int".
    ctype: str | None = None
    # The tokens of the default argument, or None if there is none.
    default: tuple[str, ...] | None = None
    is_pack: bool = False
    # The argument that the template is instantiated with: the value of an "int" or "bool" parameter, the
    # canonical C++ spelling of a "type" parameter. None before that, and for an "other" parameter.
    value: int | bool | str | None = None
    # Name of the numpy scalar type of a "type" argument ("int16"), or None if numpy has no such type.
    numpy_type: str | None = None

    @property
    def pytypes(self) -> tuple[type, ...]:
        """The class annotations that the parameter can have. Empty if Python cannot set it."""
        match self.kind:
            case "type":
                # A numpy type or the name of a C++ type.
                return (type, str)
            case "int":
                return (int,)
            case "bool":
                return (bool,)
            case _:
                return ()

    @property
    def cpp_val(self) -> str:
        """The value of an "int" or "bool" parameter as a C++ literal."""
        if self.kind == "bool":
            return "true" if self.value else "false"
        unsigned = self.ctype is not None and self.ctype.startswith("unsigned")
        return f"{self.value}U" if unsigned else str(self.value)


@dataclass
class CppArgument:
    """An argument of a constructor or of a method of a C++ module."""

    # Name of the Python keyword argument.
    name: str
    # The C++ type that the binding takes the argument as.
    ctype: str
    # The default argument as it is written in the declaration, or None if there is none.
    default: str | None = None
    # A literal is also the default in Python. Any other expression is only evaluated by the C++ class.
    default_is_literal: bool = False
    # An argument that the C++ function takes by rvalue reference: the binding takes a value and moves it.
    move: bool = False


@dataclass
class CppOverload:
    """One binding of a function: its leading arguments, with or without their defaults."""

    arguments: list[CppArgument]
    defaults: bool

    @property
    def types(self) -> list[str]:
        """The C++ types of the arguments."""
        return [a.ctype for a in self.arguments]

    @property
    def parameters(self) -> list[str]:
        """The parameter declarations of a function that takes the arguments."""
        return [f"{a.ctype} a{i}" for i, a in enumerate(self.arguments)]

    @property
    def call(self) -> str:
        """The argument list that passes those parameters on."""
        return ", ".join(
            f"std::move(a{i})" if a.move else f"a{i}"
            for i, a in enumerate(self.arguments)
        )

    @property
    def nb_args(self) -> list[str]:
        """The nanobind annotations of the arguments."""
        return [
            f'nanobind::arg("{a.name}")'
            + (f" = {a.default}" if self.defaults and a.default is not None else "")
            for a in self.arguments
        ]


@dataclass
class CppFunction:
    """A constructor (without its leading ModuleName argument) or a method of a C++ module."""

    # Name of the method. Empty for a constructor.
    name: str
    # The declaration, for messages: "set_gain(int)".
    signature: str
    arguments: list[CppArgument]
    is_static: bool = False
    # The nanobind return value policy, if it is not the default.
    return_policy: str | None = None
    # For a method that returns a dspsim::Task<T>, which Python awaits: the C++ type T ("void", "bool", ...).
    task_type: str | None = None
    # The nanobind/stl headers that the types of the function need.
    headers: set[str] = field(default_factory=set)
    # The classes and enumerations that must have Python bindings when the module is imported.
    classes: list[str] = field(default_factory=list)

    @property
    def overloads(self) -> list[CppOverload]:
        """
        The bindings of the function. Literal defaults become Python defaults. If a default is not a literal,
        there is one binding per number of arguments instead, and the C++ function supplies the defaults.
        """
        defaults = [a for a in self.arguments if a.default is not None]
        if all(a.default_is_literal for a in defaults):
            return [CppOverload(self.arguments, defaults=True)]
        count = len(self.arguments)
        return [
            CppOverload(self.arguments[:n], defaults=False)
            for n in range(count, count - len(defaults) - 1, -1)
        ]

    @property
    def task_name(self) -> str:
        """Name of the Python class of the awaitable, if this module is the first to bind it."""
        assert self.task_type is not None
        return "_Task_" + re.sub(r"\W+", "_", self.task_type).strip("_")

    @property
    def guard(self) -> str:
        """A C++ expression that is true if every class that the function uses has Python bindings."""
        return " && ".join(f"nanobind::type<{c}>().is_valid()" for c in self.classes)


@dataclass
class CppSkipped:
    """A public constructor or method that is not bound."""

    signature: str
    reason: str


@dataclass
class CppModuleInfo:
    """A C++ dspsim Module, as read from its header."""

    # Name of the Python class.
    name: str
    # Qualified name of the C++ class or class template, e.g. "::demo::Gain".
    cpp_name: str
    # The C++ type of the module: the class, or the instance of the class template ("::demo::Gain<short, 3>").
    cpp_type: str
    # The header that defines the class.
    header: Path
    include_dirs: list[Path]
    # Template parameters in declaration order, with the arguments of cpp_type. Empty if not a template.
    parameters: dict[str, CppTemplateParameter]
    # Public ports, base classes first, in declaration order.
    ports: dict[str, CppPort]
    # The public constructors that take a ModuleName first, and the public methods.
    constructors: list[CppFunction] = field(default_factory=list)
    methods: list[CppFunction] = field(default_factory=list)
    # What is not bound, and why.
    skipped: list[CppSkipped] = field(default_factory=list)
    # A final class cannot get a class of its own per build (see the bindings template).
    is_final: bool = False
    # What the module is built with besides the header. Source files are compiled into the extension module,
    # compile definitions are NAME: value (None for a definition without a value).
    sources: list[Path] = field(default_factory=list)
    definitions: dict[str, str | None] = field(default_factory=dict)
    # CMake packages (the arguments of find_package) and what the module links to (targets or libraries).
    packages: list[str] = field(default_factory=list)
    libraries: list[str] = field(default_factory=list)
    # A header with the user's own bindings: it defines dspsim_bind(), which the generated code calls.
    bindings: Path | None = None
    # The name of this build, which its extension module and its C++ class are named after: the name of the
    # Python class and a hash of everything that the build depends on. Set by the builder.
    build_name: str = ""

    @property
    def value_parameters(self) -> list[CppTemplateParameter]:
        """The template parameters whose argument is a value that Python can read."""
        return [p for p in self.parameters.values() if p.kind in ("int", "bool")]

    @property
    def type_parameters(self) -> list[CppTemplateParameter]:
        """The template parameters whose argument is a type."""
        return [p for p in self.parameters.values() if p.kind == "type"]

    @property
    def has_tasks(self) -> bool:
        """Whether a method returns a dspsim::Task."""
        return any(m.task_type is not None for m in self.methods)

    @property
    def headers(self) -> list[str]:
        """The nanobind/stl headers that the bindings need."""
        functions = self.constructors + self.methods
        return sorted(set().union(*(f.headers for f in functions)))
