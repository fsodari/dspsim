# JIT builder for C++ modules

`dspsim.builder.cppbuilder` builds the Python bindings of a C++ dspsim `Module` from its header, the
way `vbuilder` does for a SystemVerilog module. There is no binding code to write:

```cpp
// accumulator.h
#include <dspsim/module.h>
#include <dspsim/port.h>

DSPSIM_MODULE(Accumulator)
{
public:
    dspsim::Input<uint8_t> clk{"clk"};
    dspsim::Input<int16_t> d{"d"};
    dspsim::Output<int32_t> q{"q"};

    DSPSIM_CTOR(Accumulator)
    {
        DSPSIM_CORO(eval);
    }

    dspsim::Task<> eval();
};
```

```python
from dspsim.builder import CppModule, cppbuilder
from dspsim.framework import InputS16, InputU8, OutputS32


@cppbuilder(header=Path("accumulator.h"))
class Accumulator(CppModule):
    # Optional. If ports are annotated, they must match the C++ module exactly.
    clk: InputU8
    d: InputS16
    q: OutputS32


acc = Accumulator("acc")
acc.clk.bind(clk)
```

`build_cppmodule(name, header, ...)` is the same without the decorator, with the template parameters
and the port types as arguments (`parameters=`, `ports=`).

- `header` is the header that declares the class. The class can be a class or a class template, and
  its member functions can be defined in the header or in source files (see
  [Sources and dependencies](#sources-and-dependencies)).
- `cls` names the C++ class, with or without its namespace (`"demo::Scaler"` or `"Scaler"`). It
  defaults to the name of the Python class.
- `include_dirs` are added to the include path; the header's own directory and the dspsim headers
  are always on it.

The class that the decorator returns is the one from the extension module, not the class that was
decorated: the body of that only describes it. Besides the annotations, it can declare methods for
the type checker, which knows nothing about the C++ class:

```python
@cppbuilder(header=Path("mixer.h"), sources=[Path("mixer.cpp")])
class Mixer(CppModule):
    clk: InputU8

    def gain(self) -> int: ...
```

## What is bound

| C++ | Python |
|---|---|
| Public constructors whose first argument is a `dspsim::ModuleName` (`DSPSIM_CTOR` declares the one with no other argument) | `Counter("name")`, `Counter("name", 5, limit=22)` |
| Public members of type `Input<T>`, `Output<T>`, `InputArray<T>`, `OutputArray<T>`, including those of base classes | Read-only attributes (`.def_ro`) of the framework port types |
| Public members of a class derived from `dspsim::PortStruct` (a struct port) or `dspsim::PortStructArray` (an array of structs, such as `PortStructArrayOf<S>`), including those of base classes | Read-only attributes of type `PortStruct` or `PortStructArray`, which give the fields by name |
| Public methods, static or not, including those of base classes | Methods |
| Template arguments (see [Class templates](#class-templates)) | Class attributes |

A port is usable from Python if the framework binds its type: `uint8_t` to `uint64_t`, `int8_t` to
`int64_t` and `double` (`InputU8`, `OutputArrayS16`, `InputFloat`, ...). A port of another type is
still bound, but reading the attribute raises `TypeError`.

A struct port is annotated as `PortStruct`, and an array of structs as `PortStructArray`. Their fields are not parsed: Python reads them from the object
(`stage.bus_in.fields`, `stage.bus_in.data`), so they are whatever the constructor of the struct adds, and
the class can be an instance of a class template. `stage.bus_in.bind(signals)` binds it as a whole. See
[VerilatedPorts.md](VerilatedPorts.md#struct-ports). A port of more than 64 bits is an
`InputArray<uint32_t>` / `OutputArray<uint32_t>` of 32-bit words, like that of a verilated module.

The class must be declared in `header` itself, not in a file that it includes.

## Constructors and methods

```cpp
struct Counter : public dspsim::Module
{
    Counter(dspsim::ModuleName name);
    Counter(dspsim::ModuleName name, int step, int limit = 100);

    int step() const;
    void load(int value);
    void load(const std::vector<int> &values);
    void reset(int value = start, bool keep_step = true);
    dspsim::Output<int32_t> &output();
    static int version();
    ...
};
```

```python
Counter = build_cppmodule("Counter", Path("counter.h"))

counter = Counter("counter", 5, limit=22)
counter.load([1, 2, 3])
counter.reset()
assert counter.output() is counter.q
```

- **Constructors** are bound with `nanobind::new_(&dspsim::create_module<M, Args...>)`, where
  `Args` are the types of the arguments after the name. The name is the argument `name` in Python.
  Constructors that do not take a `ModuleName` first are for C++ code and are ignored.
- **Arguments** keep their names as keyword arguments (`arg0`, `arg1`, ... if they have none).
- **Default arguments** that are literals (`100`, `-1.5`, `"text"`, `true`) are the defaults in
  Python. Any other default (`start`, `MAX`, `{}`) is an expression that only the class can
  evaluate, so the function is bound once per number of arguments instead and C++ supplies the
  default: `reset(value, keep_step)`, `reset(value)`, `reset()`.
- **Overloads** are overloads in Python.
- **A method of a base class** is bound unless the class declares a method of the same name, which
  hides it in C++ as well.
- **A pointer or reference to an object** that a method returns refers to that object in Python
  (`counter.output() is counter.q`), and keeps the module alive.
- **Not methods for Python**, and left out without a warning: the methods that `DSPSIM_METHOD` and
  `DSPSIM_CORO` register as processes, operators, member function templates, and the methods of
  `dspsim::Module`, which the framework binds.

### Coroutines

A method that returns a `dspsim::Task<T>` is awaited from Python, like `AxisRx.receive()`:

```cpp
dspsim::Task<int> value_after(int cycles)
{
    for (int i = 0; i < cycles; ++i)
    {
        co_await wait(clk.pos());
    }
    co_return q.read();
}
```

```python
value = context.run_until(counter.value_after(2))


async def main():
    return await counter.value_after(2)
```

- The result type `T` follows the type table below, and `Task<>` gives `None`.
- The awaitable is a Python class per result type. The framework has those of `Task<>`,
  `Task<bool>` and `Task<std::vector<intN_t>>`, which the module uses. For any other result type
  the first module that needs it binds the class, and later modules use that one, so two modules
  with a `Task<int>` do not conflict.
- **Take the arguments by value.** A coroutine keeps its reference parameters, not what they refer
  to, and Python converts a value (a list to a `std::vector`) for the duration of the call only. A
  method like `Task<int> sum_later(const std::vector<int> &values)` would read freed memory when it
  resumes, so the builder leaves it out with a warning. References and pointers to objects that
  have Python bindings (`Signal<T> &`) are fine.
- A static method that returns a task is not bound: a task runs in the context of a module.

`methods=False` binds no methods. `methods=["load", "reset"]` binds those (all their overloads) and
raises `LookupError` for a name that the class does not have.

### Types

| Type of an argument or result | In Python |
|---|---|
| `bool`, integers, `float`, `double`, `char` | `bool`, `int`, `float`, `str` |
| `std::string`, `std::string_view`, `const char *`, `std::filesystem::path` | `str`, `pathlib.Path` |
| `std::vector`, `std::list`, `std::array`, `std::set`, `std::map`, their `unordered` variants, `std::optional`, `std::pair`, `std::tuple`, `std::variant`, `std::complex` of these types | `list`, `set`, `dict`, `None`, `tuple`, `complex`, ... |
| A class or enumeration with Python bindings (everything in `dspsim.framework`: `Signal<T>`, ports, `SensitivityEvent`, ...), by value, reference, pointer or `std::shared_ptr` | That class |

A constructor or method that uses anything else is left out, and the builder says why with a
`CppBindingWarning` (`strict=True` makes it a `TypeError`):

```
CppBindingWarning: C++ module Counter: 'fill(int *, int)' is not bound: 'int *' is a pointer to a value
CppBindingWarning: C++ module Counter: 'configure(const Opaque &)' is not bound: no Python bindings for demo::Opaque
```

- Not bound: a `dspsim::Task` as an argument, pointers and non-const references to values (`int *`, `std::vector<int> &`: Python
  could not see the change), rvalue references to classes, function pointers, `std::function`,
  `std::unique_ptr`, and classes without bindings.
- Whether a class has Python bindings is only known when the module is imported, so those warnings
  come after the build. With `strict=True`, everything else fails before it.
- To silence a warning, leave the method out with `methods=[...]`, or filter `CppBindingWarning`.

## Class templates

Template parameters are to a C++ module what parameters are to a SystemVerilog module, and they are
set the same way, with class annotations:

```cpp
template <typename T, int GAIN = 1, unsigned SHIFT = 0, bool NEGATE = false, typename W = int64_t>
DSPSIM_MODULE(Gain)
{
public:
    dspsim::Input<uint8_t> clk{"clk"};
    dspsim::Input<T> d{"d"};
    dspsim::Output<W> q{"q"};
    ...
};
```

```python
@cppbuilder(header=Path("gain.h"), cls="demo::Gain")
class Gain(CppModule):
    T: type = np.int16  # binds demo::Gain<std::int16_t, 3>
    GAIN: int = 3
    NEGATE: bool  # no value: keeps the default. Leaving it out does the same.

    clk: InputU8  # the ports are those of this instance of the template
    d: InputS16
    q: OutputS64


assert Gain.GAIN == 3 and Gain.NEGATE is False
assert Gain.T is np.int16 and Gain.W is np.int64
```

`build_cppmodule("Gain", header, cls="demo::Gain", parameters={"T": np.int16, "GAIN": 3})` takes
them as a dictionary. Each Python class is one instance of the template; decorate two classes for
two sets of arguments.

| Template parameter | Annotation | Value | Attribute of the Python class |
|---|---|---|---|
| Type (`typename T`) | `type` | A numpy type (`np.int16`, `np.float64`, `np.bool_`) or anything `np.dtype()` accepts | The numpy type |
| Type (`typename T`) | `str` | The name of a C++ type, qualified as at global scope: `"demo::Sample"` | The canonical name of the type |
| Integer (`int N`, `unsigned N`, ...) | `int` | An `int` | The value (`.def_ro_static`) |
| `bool` | `bool` | `True` or `False` | The value (`.def_ro_static`) |

- A parameter without a default needs a value. The attributes are also set for the parameters that
  keep their default, with the value that the compiler computed: a default may depend on other
  parameters (`int WIDTH = sizeof(T) * 8`).
- The annotation must be the one in the table, and the value must have that type, or the builder
  raises `TypeError`. Annotations whose names are neither template parameters nor ports are ignored.
- Other parameters (floating point values, enumerations, templates, parameter packs) cannot be set
  and are not attributes. They keep their default, and a pack is empty.
- Type arguments that are the same C++ type give the same numpy type: `"std::int16_t"` and `"short"`
  both read back as `np.int16`.
- The constructors with other arguments than the name, the methods, and a base class that depends
  on a template parameter (`struct Sink : Stream<T>`) need libclang 20, which is not what the
  `libclang` wheel has: see [libclang](#libclang). With the wheel alone, a class template is bound
  with its ports, its template arguments and the constructor that only takes the name; the rest is
  reported with a `CppBindingWarning`. A base class that is a fixed instance
  (`struct ByteSink : Stream<uint8_t>`) works with both.

## Sources and dependencies

```python
@cppbuilder(
    header=Path("mixer.h"),
    sources=[Path("mixer.cpp")],
    include_dirs=[Path("include")],
    definitions={"MIXER_GAIN": 3, "MIXER_WIDE": None},
    packages=["Eigen3"],
    libraries=["Eigen3::Eigen"],
)
class Mixer(CppModule): ...
```

| Argument | What it is | In the build |
|---|---|---|
| `sources` | Source files that define what the header declares: constructors, methods, processes | Compiled into the extension module |
| `include_dirs` | Include directories | `target_include_directories` |
| `definitions` | Compile definitions, `{"NAME": value}` or `{"NAME": None}` | `target_compile_definitions` |
| `packages` | CMake packages. Each entry is the arguments of a `find_package()`: `"Eigen3"`, `"Boost 1.80 COMPONENTS regex"` | `find_package(<entry> REQUIRED)` |
| `libraries` | What to link: the targets of the packages (`"Eigen3::Eigen"`), or libraries | `target_link_libraries` |

- The header is parsed with the same include directories and definitions as it is compiled with,
  so a port whose type depends on a definition has the right type.
- The include directories and definitions of `packages` and `libraries` are only known to CMake.
  The builder configures a small project that uses them, once per set of dependencies
  (`.dspsim_cache/dependencies_<hash>`), which writes them out.
- CMake finds packages on `CMAKE_PREFIX_PATH`: set it in the environment for packages that are not
  in a default location.
- The source files are not parsed: Python sees what the header declares. They are searched for the
  processes that the constructors register (`DSPSIM_METHOD(eval)`, `DSPSIM_CORO(eval)`), so that
  the process of a class whose constructor is defined in a source file is not bound as a method. A
  registration is taken to belong to the constructor (`Class::Class(...)`) that is defined before
  it in the file.
- The model's code is compiled with `-O3` (`Release`). nanobind compiles extension modules for size
  by default, which the builder turns off for C++ modules.

## Bindings of your own

The builder leaves out what it cannot bind: a class or enumeration of yours has no Python bindings,
and some signatures have no Python equivalent. `bindings=` names a header that binds them by hand
with [nanobind](https://nanobind.readthedocs.io). It defines `dspsim_bind()`, which the generated
bindings call:

```cpp
// hooked_bindings.h
#include "hooked.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

template <typename M>
void dspsim_bind(nanobind::module_ &m, nanobind::class_<M, dspsim::Module> &cls)
{
    namespace nb = nanobind;

    // Classes that the constructors and methods of the module use: Hooked.Config, Hooked.Mode.
    nb::class_<demo::Config>(cls, "Config")
        .def(nb::init<>())
        .def_rw("gain", &demo::Config::gain);
    nb::enum_<demo::Mode>(cls, "Mode")
        .value("Bypass", demo::Mode::Bypass)
        .value("Scale", demo::Mode::Scale);

    // A method in a form that Python can call.
    cls.def("fill", [](M &self, int n)
            {
                std::vector<int> values(n);
                self.fill(values.data(), n);
                return values; }, nb::arg("n"));
}
```

```python
Hooked = build_cppmodule("Hooked", Path("hooked.h"), bindings=Path("hooked_bindings.h"))

config = Hooked.Config()
config.gain = 3
hooked = Hooked("hooked", config)  # Hooked(ModuleName, const Config &)
hooked.set_mode(Hooked.Mode.Bypass)  # void set_mode(Mode)
```

- **It runs first**, right after the class and its ports are bound. The constructors and methods
  that use `Config` and `Mode` are bound after it, because those classes have bindings by then.
  Nothing else is needed for them.
- **`cls` is the Python class of the module, and `M` its C++ class**: a class of the build that
  derives from yours (see [Builds](#builds)). A function that takes the module takes an `M &`, not
  a `demo::Hooked &`, which Python has no objects of. A member function of your class is bound as
  it is: `cls.def("apply", &demo::Hooked::apply)`.
- **Scope**: a class bound in `cls` is a member of the Python class (`Hooked.Config`). One bound in
  `m` is in the extension module, which is `sys.modules[Hooked.__module__]`.
- **A method that the builder also binds** becomes an overload. Leave the builder's out with
  `methods=[...]`, which also silences the warning for a signature that it cannot bind.
- **A class has one Python class per process.** If two extension modules bind the same class, the
  second must not bind it again: nanobind warns for a class and aborts for an enumeration. Test
  first, and use the class that exists:

  ```cpp
  if (!nb::type<demo::Config>().is_valid())
      nb::class_<demo::Config>(cls, "Config").def(nb::init<>());
  else
      cls.attr("Config") = nb::type<demo::Config>();
  ```

  For the same reason, restart Python after changing the layout of a class that the header binds.
- The header is compiled with the module and is part of the build key, but it is not parsed.

## In a library

A package of models builds its C++ modules into its extension module, next to its verilated models,
instead of building them when they are first used. `dspsim_add_module()` reads them from the
`[tool.dspsim.cpp]` table of the `pyproject.toml` and binds them exactly as `cppbuilder` does. See
`examples/some_example`.

```toml
[build-system]
requires = ["scikit-build-core", "nanobind", "nanobind-backend", "dspsim[cpp]"]

# What the C++ modules are built with. They are compiled in one target, so this is common to all.
[tool.dspsim.cpp]
include_dirs = ["src/my_package/cpp"]
sources = ["src/my_package/cpp/*.cpp"]
definitions = { MIXER_GAIN = 3, MIXER_WIDE = true }   # true: defined without a value
packages = ["Eigen3"]
libraries = ["Eigen3::Eigen"]
bindings = "src/my_package/cpp/bindings.h"
strict = false

# One table per Python class.
[tool.dspsim.cpp.models.Counter]
header = "src/my_package/cpp/Counter.h"
cls = "demo::Counter"                     # default: the name of the table
methods = ["step", "set_step"]            # default: all of them

[tool.dspsim.cpp.models.Delay8]
header = "src/my_package/cpp/Counter.h"
cls = "demo::Delay"
parameters = { T = "std::int8_t", MAX = 4 }
```

- Paths and globs are relative to the directory of the `pyproject.toml`.
- **Template arguments** are C++ type names for type parameters, since TOML has no numpy types, and
  integers or booleans for the others.
- **One bindings header for the library.** `dspsim_bind()` is called for every C++ module, so it
  tells them apart by `M`: `if constexpr (std::is_base_of_v<demo::Hooked, M>) { ... }`.
- **The headers are parsed when CMake configures**, and CMake configures again when one of them,
  a file that they include, a source file or the bindings header changes. The package needs
  `dspsim[cpp]` to build, not to be used.
- **What cannot be bound** is printed in the output of the configure step (`dspsim: C++ module
  Counter: 'fill(int *, int)' is not bound: ...`), and fails the build with `strict = true`. What
  uses a class without Python bindings is only known when the library is imported: it is left out
  and listed in the extension module's `_unbound`, as `(class, signature)`.
- **Type stubs** are generated for the extension module, so the C++ modules are typed, with their
  constructors, ports and methods. The JIT builder has no stubs: declare methods in the decorated
  class.
- The name of a C++ module must not be the name of a verilated model of the library.

## libclang

The header is parsed with [libclang](https://clang.llvm.org/docs/LibClang.html), which sees the code
after preprocessing: `DSPSIM_MODULE` and `DSPSIM_CTOR` are ordinary declarations by then. It is an
optional dependency, like Verilator is for `vbuilder`:

```bash
pip install dspsim[cpp]     # or: pip install libclang
```

The `libclang` wheel contains the library and its Python bindings (`clang.cindex`) for Linux, macOS
and Windows, so nothing else has to be installed. It is libclang 18, which is enough for everything
except the members of class templates:

| | libclang 18 (the wheel) | libclang 20 or newer |
|---|---|---|
| Classes: ports, constructors, methods | yes | yes |
| Class templates: template arguments, ports, the constructor that only takes the name | yes | yes |
| Class templates: other constructors, methods, base classes that depend on a template parameter | no | yes |

A newer libclang is used automatically if it is installed where the table below says: the builder
looks there for a libclang 20 or newer and prefers it to the wheel's. The Python bindings still come
from the wheel, so `dspsim[cpp]` is needed either way.

| Platform | Install | Library |
|---|---|---|
| Debian, Ubuntu | `apt install libclang1-21` (or a newer version from [apt.llvm.org](https://apt.llvm.org)) | `/usr/lib/llvm-21/lib/libclang-21.so.1` |
| Fedora | `dnf install clang-libs` | `/usr/lib64/libclang.so.<version>` |
| macOS | `brew install llvm` | `/opt/homebrew/opt/llvm/lib/libclang.dylib` (`/usr/local/opt/llvm` on Intel) |
| Windows | `winget install LLVM.LLVM` (or the installer from the [LLVM releases](https://github.com/llvm/llvm-project/releases)) | `C:\Program Files\LLVM\bin\libclang.dll` |

`DSPSIM_LIBCLANG` (in the environment or in a `.env` file) overrides the choice: the path of a
library anywhere else, or `bundled` for the one in the wheel. `dspsim.cpp_parser.libclang_file()`
returns the library in use, and `supports_template_members()` whether it reads the members of class
templates.

Do not install the `clang` package from PyPI next to `libclang`: both provide the `clang` Python
module, and `clang` comes without the library.

The parser needs the system headers of the compiler that builds the module. On Linux and macOS it
asks the compiler for its search path (`$CXX`, else `c++`). On Windows it uses the `INCLUDE`
environment variable of a Visual Studio developer prompt, and outside of one libclang finds the
newest Visual Studio by itself, like CMake does.

A standard library can be newer than the libclang that reads it. Microsoft's and others use compiler
builtins in a few declarations (recent type traits) as soon as the newest compilers have them, and
the libclang 18 of the wheel does not know those. The builder then warns once
(`CppParseWarning: libclang could not parse 14 declarations of the standard library ...`) and reads
the module without them, which works unless the module's own interface depends on those
declarations. An error in your own headers is always an error. Installing a newer libclang removes
the warning. As of this writing the wheel's libclang reads the standard libraries of GCC 15,
Visual Studio 2022 and Xcode 16 without it, and warns for Visual Studio 2026 and Xcode 26.

## How it works

1. `CppHeader` ([cpp_parser.py](../src/dspsim/cpp_parser.py)) parses the header.
   `template_parameters(cls)` gives the template parameters of a class template, which the
   decorator checks the annotations against. `module_info(cls, parameters, methods)` returns a
   `CppModuleInfo` ([module_info.py](../src/dspsim/module_info.py)): the C++ type to bind, the
   template arguments, the ports, the constructors and methods, and what is left out and why. For a
   class template it parses a second translation unit that instantiates the template with the
   arguments, so the members have their real types and every default is computed by the compiler.
   It raises if the class is not found, is not a `dspsim::Module`, or has no constructor that the
   builder can bind. Compile errors in the header or in the template arguments are reported here,
   before anything is built. `load_cpp_module_info()` does both steps for one class.
2. `build_cppmodule()` ([builder.py](../src/dspsim/builder.py)) checks the port annotations and
   renders the `cpp_module_*.jinja` templates into a CMake project in the cache directory
   (`.dspsim_cache/<name>_<key>`, see [Builds](#builds) and `DSPSIM_CACHE_DIR`): the bindings, the
   extension module and a `CMakeLists.txt`.
3. The project is built with CMake, imported, and linked to the framework with
   `dspsim.link_module()`, as for a verilated model.

The generated files are only rewritten when their content changes.

The generated bindings only use headers that are installed with dspsim: the public headers, and
[`<dspsim/bindings/task.h>`](../src/include/dspsim/bindings/task.h) for methods that return a task.

The parser decides what can be bound from the types alone, with one exception. Whether a class has
Python bindings depends on what is loaded, so the generated code checks that when the module is
imported (`nanobind::type<T>().is_valid()`), leaves out what uses a class without bindings, and lists
it in the module's `_unbound` for the builder to report.

## Builds

Every build has a key: a hash of the description of the module (the name of the Python class, the C++
class and its template arguments, what is bound, the include directories, definitions, packages and
libraries) and of the contents of its files: the header and every file it includes, apart from the
system's and dspsim's, the source files and the bindings header. The cache directory and
the extension module are named after it: `.dspsim_cache/Counter_3c5722a02380/` holds
`_Counter_3c5722a02380`. `vbuilder` does the same with the model's parameters, its trace format
and every file that Verilator read.

- **The same build again** (running a notebook cell twice, the same decorator in two test files)
  is found in the cache and imported: nothing is generated or compiled. In the same process it is
  the class that is already imported, in about 20 ms. In a new process the header is parsed again,
  which is how the builder knows that the module is the same: about 0.7 s for a class and 1.2 s for
  a class template (two parses). A verilated model takes about 0.1 s either way.
- **Anything different is another build**, next to the first: another header that defines a class
  of the same name, an edited header or one of its includes, other template arguments, other
  `methods`. Both can be loaded in one process, each with its own behavior. So after editing a
  header, running the decorator again gives the new class without restarting Python; objects of the
  old class keep the old code.
- **The name of the Python class is part of the key.** Two classes that differ only in their name
  are two builds, so that each is the class it was declared as (`__name__`, `repr()`, error
  messages), whichever was built first.
- **Everything else that a module depends on is checked by the build system**, every time: the
  headers of the system and of other libraries, what only the source files or the bindings header
  include, the dspsim headers and library, and for a verilated model whatever Verilator's build
  rules track. The build step always runs. The build system has
  recorded every header that the compiler read, so for a module that is up to date this takes a few
  milliseconds, and when one of them has changed the module is rebuilt in place, in the same
  directory.
- If such a rebuild happens to a module that the process has already imported, the builder raises
  `RuntimeError`: an extension module cannot be loaded twice, so restart Python.
- The configure step of CMake only runs for a new build, and again when the compiler or the versions
  of dspsim, nanobind, CMake or Verilator change (a stamp in the build directory tells).

nanobind keeps one Python class per C++ class, so the bindings derive a class of their own from the
module's class for every build (`dspsim::generated::Counter_3c5722a02380`) and bind that. Methods
that take or return a reference to the module itself (`Counter &self()`,
`void copy_from(const Counter &)`) work with it. A module that is a `final` class cannot be derived
from and is bound as it is: a second, different build of it in the same process raises
`RuntimeError`.

Builds are never removed: an edited header leaves its earlier builds in the cache directory, which
can be deleted at any time.
