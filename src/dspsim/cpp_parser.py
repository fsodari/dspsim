"""
Read the description of a C++ dspsim Module from its header with libclang.

This is for C++ modules what `verilator --json-only` (verilator.py) is for SystemVerilog modules. The
header is parsed with the flags it will be compiled with, so macros such as DSPSIM_MODULE and DSPSIM_CTOR
are already expanded in what the parser sees. See docs/CppBuilder.md.
"""

import copy
import ctypes
import functools
import glob
import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import warnings
from collections.abc import Sequence
from pathlib import Path
from typing import Any, ClassVar

import numpy as np

import dspsim
from dspsim.module_info import (
    CppArgument,
    CppFunction,
    CppModuleInfo,
    CppPort,
    CppSkipped,
    CppTemplateParameter,
)

# The dspsim port classes, as libclang spells their canonical types: "dspsim::InputArray<short>".
_PORT_TYPE = re.compile(
    r"dspsim::(?P<direction>Input|Output)(?P<array>Array)?<(?P<T>.+)>"
)

# The C++ types that have a numpy scalar type, by the name of the numpy type.
_NUMPY_TYPES = {
    "uint8": "std::uint8_t",
    "uint16": "std::uint16_t",
    "uint32": "std::uint32_t",
    "uint64": "std::uint64_t",
    "int8": "std::int8_t",
    "int16": "std::int16_t",
    "int32": "std::int32_t",
    "int64": "std::int64_t",
    "float32": "float",
    "float64": "double",
    "bool": "bool",
}

# The element types that the framework binds ports for, with the suffix of the Python class names.
_PORT_SUFFIXES = {
    "uint8": "U8",
    "uint16": "U16",
    "uint32": "U32",
    "uint64": "U64",
    "int8": "S8",
    "int16": "S16",
    "int32": "S32",
    "int64": "S64",
    "float64": "Float",
}

# The standard library types that nanobind converts, with the header of the type caster in nanobind/stl and
# the number of template arguments that are types Python sees (None: all of them).
_STL_CASTERS: dict[str, tuple[str, int | None]] = {
    "std::basic_string": ("string.h", 0),
    "std::basic_string_view": ("string_view.h", 0),
    "std::filesystem::path": ("filesystem.h", 0),
    "std::complex": ("complex.h", 0),
    "std::vector": ("vector.h", 1),
    "std::list": ("list.h", 1),
    "std::array": ("array.h", 1),
    "std::set": ("set.h", 1),
    "std::unordered_set": ("unordered_set.h", 1),
    "std::map": ("map.h", 2),
    "std::unordered_map": ("unordered_map.h", 2),
    "std::optional": ("optional.h", 1),
    "std::pair": ("pair.h", 2),
    "std::tuple": ("tuple.h", None),
    "std::variant": ("variant.h", None),
    "std::shared_ptr": ("shared_ptr.h", 1),
}

# The macros that register a method of a module as a process.
_PROCESS_MACROS = ("DSPSIM_METHOD", "DSPSIM_CORO")
# In the text of a source file: a registration, and the definition of a constructor, Class::Class( or
# Class<T>::Class(, that it is in.
_PROCESS_REGISTRATION = re.compile(r"\b(?:DSPSIM_METHOD|DSPSIM_CORO)\s*\(\s*(\w+)\s*\)")
_CONSTRUCTOR_DEFINITION = re.compile(r"\b(\w+)\s*(?:<[^;{}()]*>)?\s*::\s*\1\s*\(")

# Reading the members of an instance of a class template takes libclang 20.
_MEMBER_VISITOR = "clang_visitCXXMethods"
_BASE_VISITOR = "clang_visitCXXBaseClasses"
_NEEDS_LIBCLANG_20 = (
    "the constructors and methods of a class template need libclang 20 or newer "
    "(see docs/CppBuilder.md)"
)

# Name of the translation unit that is parsed. It only exists in memory.
_PROBE_FILE = "dspsim_probe.cpp"
_PROBE_NAMESPACE = "dspsim_probe"
# The alias of the module type in the probe namespace, and the prefix of the aliases of the numpy types.
_PROBE_MODULE = "Module"
_PROBE_TYPE_PREFIX = "t_"


class CppParseError(RuntimeError):
    """The header could not be parsed."""


class CppParseWarning(UserWarning):
    """libclang could not parse a part of the standard library, which is newer than it is."""


class _UnsupportedType(Exception):
    """A type that the bindings cannot convert. The message says why."""


def _system_libraries() -> list[str]:
    """The libclang libraries installed in the usual places of this platform, newest first."""
    if sys.platform == "win32":
        patterns = [r"C:\Program Files\LLVM\bin\libclang.dll"]
    elif sys.platform == "darwin":
        patterns = [
            "/opt/homebrew/opt/llvm/lib/libclang.dylib",
            "/usr/local/opt/llvm/lib/libclang.dylib",
        ]
    else:
        patterns = [
            "/usr/lib/llvm-*/lib/libclang*.so*",
            "/usr/lib/*-linux-gnu/libclang*.so*",
            "/usr/lib64/libclang*.so*",
            "/usr/lib/libclang*.so*",
        ]

    # libclang.so.18.1, libclang-21.so.1: not libclang-cpp.so, which is another library.
    name = re.compile(r"libclang(-\d+)?\.(so(\.[\d.]+)?|dylib|dll)")
    libraries: dict[str, int] = {}
    for pattern in patterns:
        for path in glob.glob(pattern):
            if name.fullmatch(os.path.basename(path)):
                # The version is in the name of the library or of its directory, if anywhere.
                versions = re.findall(r"(?:llvm-|libclang-|\.so\.)(\d+)", path)
                libraries.setdefault(
                    os.path.realpath(path), int(versions[0]) if versions else 0
                )
    # A library of an unknown version (0) is tried after those that are known to be new enough.
    return [
        path
        for path, version in sorted(libraries.items(), key=lambda item: -item[1])
        if version == 0 or version >= 20
    ]


def _has_symbol(library_file: str, symbol: str) -> bool:
    try:
        return hasattr(ctypes.CDLL(library_file), symbol)
    except OSError:
        return False


@functools.cache
def _cindex() -> Any:
    """
    The libclang Python bindings, with the library chosen.

    DSPSIM_LIBCLANG names the library file, or is "bundled" for the one in the libclang wheel. Without it,
    a libclang on the system that can read the members of a class template instance is preferred to a
    bundled one that cannot.
    """
    try:
        from clang import cindex
    except ImportError as e:
        raise ImportError(
            "Building C++ modules needs libclang: install dspsim[cpp] "
            "(pip install libclang). See docs/CppBuilder.md."
        ) from e

    library_file = os.getenv("DSPSIM_LIBCLANG")
    if library_file is None and not hasattr(cindex.Type, "get_methods"):
        library_file = next(
            (f for f in _system_libraries() if _has_symbol(f, _MEMBER_VISITOR)), None
        )
    if library_file and library_file != "bundled":
        cindex.Config.set_library_file(library_file)
    return cindex


def libclang_file() -> str:
    """The libclang library that headers are parsed with."""
    return _cindex().conf.get_filename()


def supports_template_members() -> bool:
    """
    Whether libclang can read the constructors and methods of an instance of a class template (libclang 20).
    Without that, a class template is bound with its ports and its ModuleName constructor only.
    """
    cindex = _cindex()
    return hasattr(cindex.Type, "get_methods") or hasattr(
        cindex.conf.lib, _MEMBER_VISITOR
    )


def _visit(ctype: Any, method: str, function: str) -> list[Any]:
    """
    The cursors that a libclang 20 visitor of a type reports: `method` in newer Python bindings, the C
    function `function` in older ones, which take the same kind of callback for the fields of a type.
    """
    cindex = _cindex()
    if hasattr(ctype, method):
        return list(getattr(ctype, method)())

    def visitor(cursor: Any, cursors: list[Any]) -> int:
        # Keep the translation unit alive for as long as the cursor.
        cursor._tu = ctype._tu
        cursors.append(cursor)
        return 1  # continue

    callback = cindex.callbacks["fields_visit"]
    visit = getattr(cindex.conf.lib, function)
    visit.argtypes = [cindex.Type, callback, ctypes.py_object]
    visit.restype = ctypes.c_uint
    cursors: list[Any] = []
    visit(ctype, callback(visitor), cursors)
    return cursors


@functools.cache
def _compiler_include_dirs() -> tuple[str, ...]:
    """
    The directories that the C++ compiler searches for system headers.

    libclang finds the standard library on its own, but the wheel does not ship the compiler's builtin
    headers (<stddef.h>, ...), so the parser is given the search path of the compiler that builds the module.
    """
    if sys.platform == "win32":
        # Set by the Visual Studio developer prompt.
        return tuple(p for p in os.environ.get("INCLUDE", "").split(os.pathsep) if p)

    # The compiler that CMake picks.
    compiler = os.environ.get("CXX") or shutil.which("c++") or shutil.which("g++")
    if not compiler:
        return ()
    out = subprocess.run(
        [*shlex.split(compiler), "-E", "-x", "c++", "-v", os.devnull],
        check=False,
        capture_output=True,
        text=True,
    )
    search_list = re.search(
        r"#include <\.\.\.> search starts here:\n(.*?)End of search list",
        out.stderr,
        re.DOTALL,
    )
    if search_list is None:
        return ()
    dirs = (line.strip() for line in search_list.group(1).splitlines())
    # Apple's compilers also list framework directories, which are not include directories.
    return tuple(d for d in dirs if d and not d.endswith("(framework directory)"))


def _compile_args(
    header: Path,
    include_dirs: list[Path],
    definitions: dict[str, str | None],
    compile_args: list[str],
) -> list[str]:
    """The compiler arguments to parse a module header with: what the generated CMake project uses."""
    args = ["-x", "c++", "-std=c++23"]
    if sys.platform == "win32":
        # Microsoft's standard library refuses a Clang that is older than the one it was tested with, which
        # the libclang of the wheel is. The parser only reads declarations: let it through.
        args.append("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH")
    args += [f"-I{d}" for d in (dspsim.include_dir(), header.parent, *include_dirs)]
    args += [f"-D{k}" if v is None else f"-D{k}={v}" for k, v in definitions.items()]
    args += compile_args
    args += [f"-isystem{d}" for d in _compiler_include_dirs()]
    return args


def _parse(header: Path, compile_args: list[str], declarations: str = "") -> Any:
    """
    Parse a translation unit that includes the header and has the declarations in the probe namespace.

    The aliases of the numpy types are always declared: they give the canonical spelling of those types on
    this platform.
    """
    cindex = _cindex()
    aliases = "".join(
        f"using {_PROBE_TYPE_PREFIX}{name} = {ctype}; "
        for name, ctype in _NUMPY_TYPES.items()
    )
    probe = (
        f'#include "{header.as_posix()}"\n'
        "#include <cstdint>\n"
        f"namespace {_PROBE_NAMESPACE} {{ {aliases}{declarations} }}\n"
    )
    translation_unit = cindex.Index.create().parse(
        _PROBE_FILE,
        args=compile_args,
        unsaved_files=[(_PROBE_FILE, probe)],
        options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES,
    )
    errors: list[str] = []
    system_errors: list[str] = []
    for diagnostic in translation_unit.diagnostics:
        if diagnostic.severity < cindex.Diagnostic.Error:
            continue
        location = diagnostic.location
        message = f"{location.file}:{location.line}: {diagnostic.spelling}"
        # An error in a header of the system is in a declaration of the standard library that needs a newer
        # compiler than this libclang is (a new builtin, in a type trait that uses it). The declarations
        # around it are read all the same, and so is the module. An error that stops the parser is fatal.
        is_system = location.is_in_system_header
        if is_system and diagnostic.severity == cindex.Diagnostic.Error:
            system_errors.append(message)
        else:
            errors.append(message)

    if errors:
        if declarations:
            errors.append(f"while parsing: {declarations}")
        if system_errors:
            errors.append(
                f"and {len(system_errors)} errors in system headers, the first: {system_errors[0]}"
            )
        raise CppParseError(f"Could not parse {header}:\n" + "\n".join(errors))
    if system_errors:
        warnings.warn(
            f"libclang could not parse {len(system_errors)} declarations of the standard library, "
            f"which is newer than it is (the first: {system_errors[0]}). The module is read without "
            "them. A newer libclang avoids this: see docs/CppBuilder.md.",
            CppParseWarning,
            skip_file_prefixes=(str(Path(__file__).parent),),
        )
    return translation_unit


def _kind(cursor: Any) -> Any:
    """The kind of a cursor, or None for a kind that the Python bindings do not know."""
    try:
        return cursor.kind
    except ValueError:
        # The bindings raise for cursor kinds that are newer than they are (some attributes).
        return None


def _tokens(cursor: Any) -> list[Any]:
    """
    The tokens of the source text of a declaration, where macros are not expanded.

    The extent of a class that is declared with DSPSIM_MODULE starts inside the expansion of the macro,
    which libclang gives no tokens for: tokenize the same range by its positions in the file instead.
    """
    cindex = _cindex()
    translation_unit = cursor.translation_unit
    start, end = cursor.extent.start, cursor.extent.end
    extent = cindex.SourceRange.from_locations(
        cindex.SourceLocation.from_position(
            translation_unit, start.file, start.line, start.column
        ),
        cindex.SourceLocation.from_position(
            translation_unit, end.file, end.line, end.column
        ),
    )
    return list(translation_unit.get_tokens(extent=extent))


def _qualified_name(cursor: Any) -> str:
    """The qualified name of a declaration, e.g. "demo::Scaler"."""
    cindex = _cindex()
    names: list[str] = []
    while cursor is not None and _kind(cursor) != cindex.CursorKind.TRANSLATION_UNIT:
        if cursor.spelling:
            names.append(cursor.spelling)
        cursor = cursor.semantic_parent
    return "::".join(reversed(names))


def _is_in_file(cursor: Any, path: Path) -> bool:
    file = cursor.location.file
    return file is not None and Path(file.name).resolve() == path


def _find_classes(parent: Any, header: Path, name: str) -> list[Any]:
    """The classes and class templates defined in the header that the name refers to."""
    kinds = _cindex().CursorKind
    found: list[Any] = []
    for cursor in parent.get_children():
        if not _is_in_file(cursor, header):
            continue
        kind = _kind(cursor)
        if kind == kinds.NAMESPACE:
            found += _find_classes(cursor, header, name)
        elif (
            kind in (kinds.STRUCT_DECL, kinds.CLASS_DECL, kinds.CLASS_TEMPLATE)
            and cursor.is_definition()
            # A name without a namespace matches the class in any namespace.
            and name in (cursor.spelling, _qualified_name(cursor))
        ):
            found.append(cursor)
    return found


def _probe_aliases(translation_unit: Any) -> dict[str, Any]:
    """The types that the aliases in the probe namespace name, by the name of the alias."""
    kinds = _cindex().CursorKind
    for cursor in translation_unit.cursor.get_children():
        if _kind(cursor) == kinds.NAMESPACE and cursor.spelling == _PROBE_NAMESPACE:
            return {
                alias.spelling: alias.underlying_typedef_type.get_canonical()
                for alias in cursor.get_children()
                if _kind(alias) == kinds.TYPE_ALIAS_DECL
            }
    return {}


def _numpy_types(translation_unit: Any) -> dict[str, str]:
    """The numpy type of each C++ type that has one, by canonical spelling: {"unsigned char": "uint8"}."""
    return {
        ctype.spelling: alias.removeprefix(_PROBE_TYPE_PREFIX)
        for alias, ctype in _probe_aliases(translation_unit).items()
        if alias.startswith(_PROBE_TYPE_PREFIX)
    }


def _template_parameters(cls: Any) -> dict[str, CppTemplateParameter]:
    """The template parameters of a class template, in declaration order. Empty for a class."""
    cindex = _cindex()
    kinds = cindex.CursorKind
    types = cindex.TypeKind
    integers = (
        (types.SCHAR, types.SHORT, types.INT, types.LONG, types.LONGLONG)
        + (types.UCHAR, types.USHORT, types.UINT, types.ULONG, types.ULONGLONG)
    )  # fmt: skip

    parameters: dict[str, CppTemplateParameter] = {}
    for cursor in cls.get_children():
        cursor_kind = _kind(cursor)
        if cursor_kind not in (
            kinds.TEMPLATE_TYPE_PARAMETER,
            kinds.TEMPLATE_NON_TYPE_PARAMETER,
            kinds.TEMPLATE_TEMPLATE_PARAMETER,
        ):
            continue

        # The tokens are those of the declaration: "int", "SHIFT", "=", "2".
        tokens = tuple(token.spelling for token in _tokens(cursor))
        declarator = tokens[: tokens.index("=")] if "=" in tokens else tokens
        parameter = CppTemplateParameter(
            # An unnamed parameter cannot be set, but it has a place in the argument list.
            name=cursor.spelling or f"<{len(parameters)}>",
            kind="other",
            default=tokens[len(declarator) + 1 :] if "=" in tokens else None,
            is_pack="..." in declarator,
        )
        if parameter.is_pack or not cursor.spelling:
            pass
        elif cursor_kind == kinds.TEMPLATE_TYPE_PARAMETER:
            parameter.kind = "type"
        elif cursor_kind == kinds.TEMPLATE_NON_TYPE_PARAMETER:
            ctype = cursor.type.get_canonical()
            if ctype.kind == types.BOOL:
                parameter.kind = "bool"
            elif ctype.kind in integers:
                parameter.kind = "int"
            if parameter.kind != "other":
                parameter.ctype = ctype.spelling
        parameters[parameter.name] = parameter
    return parameters


def _argument(parameter: CppTemplateParameter, value: Any) -> str:
    """The C++ spelling of the template argument that a Python value stands for."""
    match parameter.kind:
        case "type":
            if isinstance(value, str):
                return value
            try:
                return _NUMPY_TYPES[np.dtype(value).name]
            except TypeError, KeyError:
                raise TypeError(
                    f"Template parameter '{parameter.name}': {value!r} is neither the name of "
                    "a C++ type nor a numpy type that has a C++ type"
                ) from None
        case "bool":
            if isinstance(value, bool):
                return "true" if value else "false"
        case "int":
            if isinstance(value, int) and not isinstance(value, bool):
                return str(value)
        case _:
            raise TypeError(
                f"Template parameter '{parameter.name}' cannot be set from Python"
            )
    raise TypeError(
        f"Type mismatch for parameter '{parameter.name}': "
        f"expected {parameter.kind}, got {type(value).__name__}"
    )


def _template_arguments(
    cpp_name: str,
    parameters: dict[str, CppTemplateParameter],
    values: dict[str, Any],
) -> list[str]:
    """
    The template argument list that sets the given parameters, each as its C++ spelling.

    Template arguments are positional, so a parameter that comes before one that is set gets its default
    argument, as it is written in the declaration. The parameters after the last one that is set are left out.
    """
    for name in values:
        if name not in parameters:
            raise TypeError(f"'{cpp_name}' has no template parameter '{name}'")

    # A value is put in parentheses where it replaces the parameter's name in an expression.
    def substitute(token: str) -> str:
        argument = arguments.get(token, token)
        is_value = token in arguments and parameters[token].kind in ("int", "bool")
        return f"({argument})" if is_value else argument

    arguments: dict[str, str] = {}
    last_set = 0
    for parameter in parameters.values():
        value = values.get(parameter.name)
        if value is not None:
            arguments[parameter.name] = _argument(parameter, value)
            last_set = len(arguments)
        elif parameter.default is not None:
            # The default may refer to the parameters before it: sizeof(T) * 8.
            default = " ".join(substitute(token) for token in parameter.default)
            is_expression = parameter.kind != "type" and len(parameter.default) > 1
            arguments[parameter.name] = f"({default})" if is_expression else default
        elif parameter.is_pack:
            # An empty pack. It is the last parameter.
            break
        else:
            raise TypeError(
                f"Template parameter '{parameter.name}' of '{cpp_name}' has no default: "
                "it needs a value"
            )
    return list(arguments.values())[:last_set]


def _resolve_arguments(
    module: Any,
    parameters: dict[str, CppTemplateParameter],
    numpy_types: dict[str, str],
) -> None:
    """Store the arguments that a class template was instantiated with in its parameters."""
    kinds = _cindex().TemplateArgumentKind
    count = module.get_num_template_arguments()
    for index, parameter in enumerate(parameters.values()):
        if index >= count or parameter.is_pack:
            break
        kind = module.get_template_argument_kind(index)
        if parameter.kind == "type" and kind == kinds.TYPE:
            ctype = module.get_template_argument_type(index).get_canonical().spelling
            parameter.value = ctype
            parameter.numpy_type = numpy_types.get(ctype)
        elif parameter.kind == "bool" and kind == kinds.INTEGRAL:
            parameter.value = bool(module.get_template_argument_value(index))
        elif parameter.kind == "int" and kind == kinds.INTEGRAL:
            assert parameter.ctype is not None
            if parameter.ctype.startswith("unsigned"):
                parameter.value = module.get_template_argument_unsigned_value(index)
            else:
                parameter.value = module.get_template_argument_value(index)


def _template_of(cls: Any) -> Any:
    """The class template that a class is an instance of, or None."""
    return _cindex().conf.lib.clang_getSpecializedCursorTemplate(cls)


def _base_classes(cls: Any) -> list[Any]:
    """The definitions of the public base classes of a class."""
    cindex = _cindex()
    kinds = cindex.CursorKind
    template = _template_of(cls)
    if template is None:
        specifiers = list(cls.get_children())
    elif supports_template_members():
        specifiers = _visit(cls.type, "get_bases", _BASE_VISITOR)
    else:
        # Older libclangs do not give the base classes of an instance of a class template: read the
        # template's. That works unless a base class depends on a template parameter.
        specifiers = list(template.get_children())

    bases: list[Any] = []
    for cursor in specifiers:
        if (
            _kind(cursor) != kinds.CXX_BASE_SPECIFIER
            or cursor.access_specifier != cindex.AccessSpecifier.PUBLIC
        ):
            continue
        base = cursor.type.get_canonical().get_declaration()
        if _kind(base) not in (kinds.STRUCT_DECL, kinds.CLASS_DECL):
            raise NotImplementedError(
                f"'{_qualified_name(cls)}' has a base class that depends on a template "
                f"parameter ({cursor.type.spelling}), which needs libclang 20 or newer "
                "(see docs/CppBuilder.md)"
            )
        bases.append(base)
    return bases


def _is_module(cls: Any) -> bool:
    """Whether the class is dspsim::Module or derives from it publicly."""
    if _qualified_name(cls) == "dspsim::Module":
        return True
    return any(_is_module(base) for base in _base_classes(cls))


def _port_struct_class(ctype: Any) -> str | None:
    """
    "PortStruct" or "PortStructArray" if a type is that dspsim class or a class that derives from it publicly:
    a struct port or an array of them. None for any other type.
    """
    kinds = _cindex().CursorKind

    def base(cls: Any) -> str | None:
        name = _qualified_name(cls)
        if name in ("dspsim::PortStruct", "dspsim::PortStructArray"):
            return name.removeprefix("dspsim::")
        return next(filter(None, map(base, _base_classes(cls))), None)

    declaration = ctype.get_canonical().get_declaration()
    if _kind(declaration) not in (kinds.STRUCT_DECL, kinds.CLASS_DECL):
        return None
    try:
        return base(declaration)
    except NotImplementedError:
        # A base class that an older libclang cannot read, of some class template of another library.
        return None


def _ports(cls: Any, numpy_types: dict[str, str]) -> dict[str, CppPort]:
    """The public port members of a class and of its base classes, base classes first."""
    cindex = _cindex()
    ports: dict[str, CppPort] = {}
    for base in _base_classes(cls):
        ports |= _ports(base, numpy_types)

    for field in cls.type.get_fields():
        if field.access_specifier != cindex.AccessSpecifier.PUBLIC:
            continue
        canonical = field.type.get_canonical().spelling
        match = _PORT_TYPE.fullmatch(canonical)
        if match is None:
            if struct_class := _port_struct_class(field.type):
                ports[field.spelling] = CppPort(
                    name=field.spelling,
                    direction=None,
                    is_array=struct_class == "PortStructArray",
                    ctype=canonical,
                    suffix=None,
                    is_struct=True,
                )
            continue
        ports[field.spelling] = CppPort(
            name=field.spelling,
            direction="input" if match["direction"] == "Input" else "output",
            is_array=match["array"] is not None,
            ctype=match["T"],
            suffix=_PORT_SUFFIXES.get(numpy_types.get(match["T"], "")),
        )
    return ports


def _is_module_name(ctype: Any) -> bool:
    """Whether a parameter type is a dspsim::ModuleName, by value or by reference."""
    spelling = ctype.get_canonical().spelling
    return spelling.removeprefix("const ").rstrip("& ") == "dspsim::ModuleName"


def _std_name(ctype: Any) -> str:
    """The name of a class without the internal namespaces of the standard library: "std::basic_string"."""
    names = _qualified_name(ctype.get_declaration()).split("::")
    return "::".join(n for n in names if not n.startswith("__"))


def _is_class(ctype: Any) -> bool:
    """Whether a type is a class that needs Python bindings of its own (not a type that nanobind converts)."""
    return (
        ctype.kind == _cindex().TypeKind.RECORD and _std_name(ctype) not in _STL_CASTERS
    )


def _unqualified(ctype: Any) -> str:
    """The spelling of a canonical type without const and volatile."""
    spelling = ctype.spelling
    while spelling.startswith(("const ", "volatile ")):
        spelling = spelling.split(" ", 1)[1]
    return spelling


def _check_type(ctype: Any, function: CppFunction, role: str) -> None:
    """
    Check that the bindings can convert a type, and record what they need for it in the function: nanobind
    headers, and classes that must have Python bindings. `role` is "argument", "return" or "element" (of a
    container). Raises _UnsupportedType otherwise.
    """
    types = _cindex().TypeKind
    values = (
        (types.BOOL, types.CHAR_S, types.SCHAR, types.UCHAR)
        + (types.SHORT, types.USHORT, types.INT, types.UINT)
        + (types.LONG, types.ULONG, types.LONGLONG, types.ULONGLONG)
        + (types.FLOAT, types.DOUBLE)
    )  # fmt: skip
    ctype = ctype.get_canonical()
    kind = ctype.kind

    def add_class(ctype: Any) -> None:
        name = _unqualified(ctype)
        if name not in function.classes:
            function.classes.append(name)

    if kind in values or (kind == types.VOID and role == "return"):
        return
    if kind == types.ENUM:
        return add_class(ctype)
    if kind in (types.LVALUEREFERENCE, types.RVALUEREFERENCE, types.POINTER):
        pointee = ctype.get_pointee()
        if kind == types.RVALUEREFERENCE:
            # A value that the function takes over. Python keeps its objects.
            if role != "argument" or _is_class(pointee):
                raise _UnsupportedType(f"'{ctype.spelling}' is an rvalue reference")
            return _check_type(pointee, function, role)
        if _is_class(pointee):
            # A reference to an object that Python has, or gets a reference to.
            return _check_type(pointee, function, role)
        if kind == types.POINTER:
            if (
                pointee.kind in (types.CHAR_S, types.CHAR_U)
                and pointee.is_const_qualified()
            ):
                return  # const char *
            raise _UnsupportedType(f"'{ctype.spelling}' is a pointer to a value")
        if role == "argument" and not pointee.is_const_qualified():
            raise _UnsupportedType(
                f"'{ctype.spelling}' is a reference to a value, which Python cannot change"
            )
        return _check_type(pointee, function, role)
    if kind == types.RECORD:
        name = _std_name(ctype)
        if name == "dspsim::Task":
            raise _UnsupportedType("a dspsim::Task can only be the result of a method")
        if name not in _STL_CASTERS:
            if name.startswith("std::"):
                raise _UnsupportedType(f"'{ctype.spelling}' has no Python type")
            return add_class(ctype)

        header, count = _STL_CASTERS[name]
        if count is None:
            count = ctype.get_num_template_arguments()
        elements = [
            ctype.get_template_argument_type(i).get_canonical() for i in range(count)
        ]
        is_string = name in ("std::basic_string", "std::basic_string_view")
        characters = (types.CHAR_S, types.CHAR_U)
        if is_string and ctype.get_template_argument_type(0).kind not in characters:
            raise _UnsupportedType(f"'{ctype.spelling}' is not a string of char")
        if name == "std::shared_ptr" and not _is_class(elements[0]):
            raise _UnsupportedType(f"'{ctype.spelling}' does not point to a class")
        function.headers.add(header)
        for element in elements:
            _check_type(element, function, "element")
        return
    raise _UnsupportedType(f"'{ctype.spelling}' has no Python type")


def _default_argument(parameter: Any) -> tuple[str | None, bool]:
    """The default argument of a function parameter as it is written, and whether it is a literal."""
    tokens = _tokens(parameter)
    spellings = [token.spelling for token in tokens]
    if "=" not in spellings:
        return None, False
    default = tokens[spellings.index("=") + 1 :]
    # A literal, with or without a sign: 3, -1.5f, "text", true.
    value = default[1:] if default and default[0].spelling in ("-", "+") else default
    is_literal = len(value) == 1 and (
        value[0].kind.name == "LITERAL" or value[0].spelling in ("true", "false")
    )
    separator = "" if is_literal else " "
    return separator.join(token.spelling for token in default), is_literal


def _refers_to(ctype: Any, cls: Any) -> bool:
    """Whether a type is a reference or a pointer to the class."""
    types = _cindex().TypeKind
    return ctype.kind in (types.LVALUEREFERENCE, types.POINTER) and _unqualified(
        ctype.get_pointee()
    ) == _unqualified(cls.type.get_canonical())


def _function(
    cursor: Any, parameters: list[Any], name: str, module: Any
) -> CppFunction:
    """
    Describe the binding of a constructor or method of the module that takes the given parameters. Raises
    _UnsupportedType if it has a type that the bindings cannot convert.
    """
    cindex = _cindex()
    types = cindex.TypeKind
    function = CppFunction(
        name=name,
        signature=cursor.displayname,
        arguments=[],
        is_static=cursor.is_static_method(),
    )
    result = None
    is_task = False
    if cursor.kind == cindex.CursorKind.CXX_METHOD:
        result = cursor.result_type.get_canonical()
        is_task = result.kind == types.RECORD and _std_name(result) == "dspsim::Task"

    # The bindings give the module a class of its own, M, derived from the module's class. A reference to
    # the module is taken as a reference to M, which is the class that Python has.
    own = _unqualified(module.type.get_canonical())

    for index, parameter in enumerate(parameters):
        ctype = parameter.type.get_canonical()
        is_own = _refers_to(ctype, module)
        if not is_own:
            _check_type(ctype, function, "argument")
        # A coroutine keeps its reference parameters, not what they refer to. Python converts a value for
        # the call only, so it is gone when the task resumes. An object with Python bindings stays.
        refers = ctype.kind in (
            types.LVALUEREFERENCE,
            types.RVALUEREFERENCE,
            types.POINTER,
        )
        if is_task and refers and not _is_class(ctype.get_pointee()):
            raise _UnsupportedType(
                f"a coroutine that takes '{ctype.spelling}' would refer to a value that is gone "
                "when it resumes: take the argument by value"
            )
        default, is_literal = _default_argument(parameter)
        move = ctype.kind == types.RVALUEREFERENCE
        function.arguments.append(
            CppArgument(
                name=parameter.spelling or f"arg{index}",
                ctype=ctype.spelling.replace(own, "M")
                if is_own
                else (ctype.get_pointee() if move else ctype).spelling,
                default=default,
                default_is_literal=is_literal,
                move=move,
            )
        )

    if is_task:
        assert result is not None
        if function.is_static:
            raise _UnsupportedType("a static method cannot return a dspsim::Task")
        # Python awaits the task, and the result of the task is the value of the await.
        task_result = result.get_template_argument_type(0).get_canonical()
        _check_type(task_result, function, "return")
        function.task_type = task_result.spelling
    elif result is not None:
        # A reference to the module itself is returned as the object that Python has: nanobind finds the
        # Python class of a polymorphic object by its dynamic type, which is M.
        if not _refers_to(result, module):
            _check_type(result, function, "return")
        # A pointer or reference to an object: Python refers to it, and it lives as long as the module does.
        if result.kind in (types.POINTER, types.LVALUEREFERENCE) and _is_class(
            result.get_pointee()
        ):
            policy = "reference" if function.is_static else "reference_internal"
            function.return_policy = f"nanobind::rv_policy::{policy}"
    return function


def _member_functions(cls: Any) -> list[Any] | None:
    """
    The constructors and methods declared in a class, or None if this libclang cannot read them: those of an
    instance of a class template, before libclang 20.
    """
    kinds = _cindex().CursorKind
    if _template_of(cls) is None:
        members = cls.get_children()
    elif supports_template_members():
        members = _visit(cls.type, "get_methods", _MEMBER_VISITOR)
    else:
        return None
    return [m for m in members if _kind(m) in (kinds.CONSTRUCTOR, kinds.CXX_METHOD)]


def _is_public(cursor: Any) -> bool:
    return (
        cursor.access_specifier == _cindex().AccessSpecifier.PUBLIC
        and not cursor.is_deleted_method()
    )


def _is_named_constructor(cursor: Any) -> bool:
    """Whether a member is a public constructor that takes a dspsim::ModuleName first."""
    parameters = list(cursor.get_arguments())
    return (
        _kind(cursor) == _cindex().CursorKind.CONSTRUCTOR
        and _is_public(cursor)
        and len(parameters) > 0
        and _is_module_name(parameters[0].type)
    )


def _is_method(cursor: Any) -> bool:
    """Whether a member is a method that Python could call: not an operator or a conversion."""
    return (
        _kind(cursor) == _cindex().CursorKind.CXX_METHOD
        and _is_public(cursor)
        and not cursor.spelling.startswith("operator")
    )


def _process_names(cls: Any, sources: list[Path]) -> set[str]:
    """
    The methods that the class registers as processes with DSPSIM_METHOD or DSPSIM_CORO: in the class, or in
    a constructor of the class that one of the source files defines.
    """
    # The tokens are those of the source text, where the macros are not expanded yet.
    tokens = [token.spelling for token in _tokens(_template_of(cls) or cls)]
    names = {
        tokens[i + 2]
        for i, token in enumerate(tokens[:-2])
        if token in _PROCESS_MACROS and tokens[i + 1] == "("
    }
    # The source files are not parsed. A registration belongs to the constructor that is defined before it.
    for source in sources:
        text = source.read_text(errors="replace")
        constructors = [
            (m.start(), m[1]) for m in _CONSTRUCTOR_DEFINITION.finditer(text)
        ]
        for registration in _PROCESS_REGISTRATION.finditer(text):
            before = [
                name for start, name in constructors if start < registration.start()
            ]
            if before and before[-1] == cls.spelling:
                names.add(registration[1])
    return names


def _is_final(cls: Any) -> bool:
    """Whether a class is declared final."""
    kinds = _cindex().CursorKind
    members = (_template_of(cls) or cls).get_children()
    return any(_kind(member) == kinds.CXX_FINAL_ATTR for member in members)


def _user_classes(cls: Any) -> list[Any]:
    """A class and its public base classes, the class first, up to but without dspsim::Module."""
    classes = [cls]
    for base in _base_classes(cls):
        if _qualified_name(base) != "dspsim::Module":
            classes += _user_classes(base)
    return classes


def _add_constructors(info: CppModuleInfo, cls: Any) -> None:
    """Describe the public constructors of the module that take a dspsim::ModuleName first."""
    members = _member_functions(cls)
    if members is None:
        # Of a class template, an older libclang only shows that the constructors exist: bind the one
        # whose only argument is the name.
        for cursor in filter(_is_named_constructor, _template_of(cls).get_children()):
            if len(list(cursor.get_arguments())) == 1:
                info.constructors.append(CppFunction("", cursor.displayname, []))
            else:
                info.skipped.append(CppSkipped(cursor.displayname, _NEEDS_LIBCLANG_20))
        return

    for cursor in filter(_is_named_constructor, members):
        try:
            parameters = list(cursor.get_arguments())[1:]
            info.constructors.append(_function(cursor, parameters, "", cls))
        except _UnsupportedType as e:
            info.skipped.append(CppSkipped(cursor.displayname, str(e)))


def _add_methods(info: CppModuleInfo, cls: Any, methods: bool | Sequence[str]) -> None:
    """Describe the public methods of the module and of its base classes that `methods` selects."""
    if methods is False:
        return
    selected = None if methods is True else set(methods)
    found: set[str] = set()
    # The names declared in the classes before: they hide the methods of that name in a base class.
    hidden: set[str] = set()
    for user_class in _user_classes(cls):
        members = _member_functions(user_class)
        if members is None:
            members = list(_template_of(user_class).get_children())
            is_readable = False
        else:
            is_readable = True
        processes = _process_names(user_class, info.sources)

        for cursor in filter(_is_method, members):
            name = cursor.spelling
            if name in hidden or name in processes:
                continue
            if selected is not None and name not in selected:
                continue
            found.add(name)
            if not is_readable:
                info.skipped.append(CppSkipped(cursor.displayname, _NEEDS_LIBCLANG_20))
                continue
            try:
                parameters = list(cursor.get_arguments())
                info.methods.append(_function(cursor, parameters, name, cls))
            except _UnsupportedType as e:
                info.skipped.append(CppSkipped(cursor.displayname, str(e)))
        hidden |= {member.spelling for member in members}

    if selected is not None and (missing := sorted(selected - found)):
        raise LookupError(f"'{info.cpp_name}' has no public method '{missing[0]}'")


def _digest(path: Path) -> bytes:
    return hashlib.sha256(path.read_bytes()).digest()


class CppHeader:
    """A parsed header that defines C++ dspsim Modules."""

    # The headers parsed in this process, by the arguments that they were parsed with. See load().
    _loaded: ClassVar[dict[str, CppHeader]] = {}

    def __init__(
        self,
        header: Path,
        include_dirs: list[Path] | None = None,
        definitions: dict[str, Any] | None = None,
        sources: list[Path] | None = None,
        compile_args: list[str] | None = None,
    ):
        """
        Parse `header` with the include directories and compile definitions that the module is built with.
        `sources` are the source files that define what the header declares: they are not parsed, only
        searched for the processes that constructors register. `compile_args` are further compiler arguments,
        such as the include directories of the libraries that the module uses.
        """
        self.header = Path(header).resolve()
        if not self.header.is_file():
            raise FileNotFoundError(f"C++ module header not found: {self.header}")
        self.include_dirs = [Path(d).resolve() for d in include_dirs or []]
        self.definitions = {
            name: None if value is None else str(value)
            for name, value in (definitions or {}).items()
        }
        self.sources = [Path(s).resolve() for s in sources or []]
        for source in self.sources:
            if not source.is_file():
                raise FileNotFoundError(f"C++ module source not found: {source}")
        self._compile_args = _compile_args(
            self.header, self.include_dirs, self.definitions, list(compile_args or [])
        )
        self._translation_unit = _parse(self.header, self._compile_args)
        # What load() needs to tell that the header is unchanged, and what it has been asked before.
        self._digests = {path: _digest(path) for path in self.source_files()}
        self._module_infos: dict[str, CppModuleInfo] = {}

    @classmethod
    def load(
        cls,
        header: Path,
        include_dirs: list[Path] | None = None,
        definitions: dict[str, Any] | None = None,
        sources: list[Path] | None = None,
        compile_args: list[str] | None = None,
    ) -> CppHeader:
        """
        Parse a header, or return it as it was parsed before in this process with the same arguments, if
        neither the header, a file that it includes nor a source file has changed since. Parsing is most of
        the time that it takes to find that a module is already built.
        """
        key = repr(
            (
                Path(header).resolve(),
                [Path(d).resolve() for d in include_dirs or []],
                sorted((definitions or {}).items()),
                [Path(s).resolve() for s in sources or []],
                compile_args or [],
            )
        )
        parsed = cls._loaded.get(key)
        if parsed is None or not parsed._is_current():
            parsed = cls._loaded[key] = cls(
                header, include_dirs, definitions, sources, compile_args
            )
        return parsed

    def _is_current(self) -> bool:
        """Whether the files that were parsed still have the contents that they had."""
        return all(
            path.is_file() and _digest(path) == digest
            for path, digest in self._digests.items()
        )

    def template_parameters(self, cls: str) -> dict[str, CppTemplateParameter]:
        """
        The template parameters of the class template `cls`, in declaration order, with their defaults.
        Empty if `cls` is not a template.
        """
        return _template_parameters(self._find_class(cls))

    def module_info(
        self,
        cls: str,
        parameters: dict[str, Any] | None = None,
        methods: bool | Sequence[str] = True,
    ) -> CppModuleInfo:
        """
        Describe the dspsim Module `cls`: a class, or a class template with the given template arguments.

        `cls` is the name of the C++ class, with or without its namespace ("demo::Scaler" or "Scaler").
        `parameters` sets template parameters by name. A type parameter takes a numpy type (np.int16) or the
        name of a C++ type; the others take an int or a bool. Parameters that are not set, or set to None,
        keep their default. `methods` selects the public methods to bind: all of them, none, or those named.
        """
        # The same question about the same header has the same answer. The caller gets a copy to change.
        selected = methods if isinstance(methods, bool) else sorted(methods)
        question = repr((cls, sorted((parameters or {}).items(), key=repr), selected))
        if question not in self._module_infos:
            self._module_infos[question] = self._module_info(cls, parameters, methods)
        return copy.deepcopy(self._module_infos[question])

    def _module_info(
        self,
        cls: str,
        parameters: dict[str, Any] | None,
        methods: bool | Sequence[str],
    ) -> CppModuleInfo:
        cindex = _cindex()
        cursor = self._find_class(cls)
        cpp_name = _qualified_name(cursor)

        template_parameters = _template_parameters(cursor)
        arguments = _template_arguments(cpp_name, template_parameters, parameters or {})
        translation_unit = self._translation_unit
        cpp_type = f"::{cpp_name}"
        module = cursor
        if cursor.kind == cindex.CursorKind.CLASS_TEMPLATE:
            # Instantiate the class template with the arguments and read the class that this gives. Its
            # member functions are not instantiated.
            cpp_type += f"<{', '.join(arguments)}>"
            translation_unit = _parse(
                self.header,
                self._compile_args,
                f"using {_PROBE_MODULE} = {cpp_type}; "
                f"inline constexpr auto module_size = sizeof({_PROBE_MODULE});",
            )
            module = _probe_aliases(translation_unit)[_PROBE_MODULE].get_declaration()
        numpy_types = _numpy_types(translation_unit)
        _resolve_arguments(module, template_parameters, numpy_types)

        if not _is_module(module):
            raise TypeError(f"'{cpp_name}' does not derive from dspsim::Module")
        if module.is_abstract_record():
            raise TypeError(f"'{cpp_name}' is an abstract class")

        info = CppModuleInfo(
            name=cursor.spelling,
            cpp_name=f"::{cpp_name}",
            cpp_type=cpp_type,
            header=self.header,
            include_dirs=self.include_dirs,
            parameters=template_parameters,
            ports=_ports(module, numpy_types),
            is_final=_is_final(module),
            sources=self.sources,
            definitions=self.definitions,
        )
        _add_constructors(info, module)
        _add_methods(info, module, methods)

        # Python creates the module with a constructor that takes the module's name first (DSPSIM_CTOR).
        if not info.constructors:
            reasons = "".join(f"\n  {s.signature}: {s.reason}" for s in info.skipped)
            raise TypeError(
                f"'{cpp_name}' has no public constructor that can be bound: it needs one whose "
                f"first argument is a dspsim::ModuleName.{reasons}"
            )
        return info

    def source_files(self) -> list[Path]:
        """
        The files of the module: the header and the files that it includes, without the headers of the system
        and of dspsim, and the source files.
        """
        excluded = [Path(d).resolve() for d in _compiler_include_dirs()]
        excluded.append(dspsim.include_dir().resolve())
        files = {self.header, *self.sources}
        for inclusion in self._translation_unit.get_includes():
            path = Path(inclusion.include.name).resolve()
            if not any(path.is_relative_to(directory) for directory in excluded):
                files.add(path)
        return sorted(files)

    def _find_class(self, cls: str) -> Any:
        """The definition of the class or class template that `cls` names."""
        name = cls.removeprefix("::")
        classes = _find_classes(self._translation_unit.cursor, self.header, name)
        if not classes:
            raise LookupError(f"No class '{cls}' is defined in {self.header}")
        if len(classes) > 1:
            candidates = ", ".join(_qualified_name(c) for c in classes)
            raise LookupError(f"'{cls}' is ambiguous in {self.header}: {candidates}")
        return classes[0]


def load_cpp_module_info(
    header: Path,
    cls: str,
    include_dirs: list[Path] | None = None,
    parameters: dict[str, Any] | None = None,
    methods: bool | Sequence[str] = True,
    definitions: dict[str, Any] | None = None,
    sources: list[Path] | None = None,
) -> CppModuleInfo:
    """Parse a header and describe the dspsim Module `cls` that it defines. See CppHeader.module_info()."""
    parsed = CppHeader(header, include_dirs, definitions, sources)
    return parsed.module_info(cls, parameters, methods)
