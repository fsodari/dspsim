# Style Guide

This document defines the development standards and architectural guidelines for this repository.

## C++ Guidelines

### Formatting
- Use the "Visual Studio" format option in VSCode
- Use 4 spaces for indents, don't use tabs
- Curly braces should go on a new line directly underneath the control statement or function declaration. "Allman style"
- "if" statemens, for-loops, while-loops must always use curly braces even for one liners.

### Naming Conventions
- Class names should use PascalCase
- Function and variable names should use snake_case
- Private member variable names should use a trailing underscore_
- Do not start a C++ identifier with an underscore. Private methods have plain names. Python bindings use a leading underscore for private names.

### Class organization
- From top to bottom, declare members in this order: types/enums, consts/constexprs, constructors/destructors, static factory functions, public methods, protected methods, private methods, public member variables, protected member variables, private member variables.
- Member variables must be declared in the order they are initialized, specifically in the order they must be initialized
- In this project specifically, if an object obtains or is passed a Context*, that should be the first member.
- Group related functions together.
- Definitions in the cpp file should match the order they were declared in the class.

### Project conventions
- There are a number of classes that reference each other. Use forward declarations when needed and don't include the declared class's header. A header includes what its own declarations need (base classes, by-value members, types used in inline code) and forward declares everything else; the `.cpp` file includes the rest. `module.h` is the one deliberate exception: it includes `context.h`, `process.h` and `coro.h`, because every module uses `DSPSIM_METHOD` / `DSPSIM_CORO` / `wait()`.
- Keep heavy headers out of the public headers: `<thread>`, `<mutex>`, `<condition_variable>`, `<format>`, `<filesystem>`, `<ranges>` and spdlog cost more to parse than all of dspsim. If a class needs one for a member, hide the member behind a pointer to a struct defined in the `.cpp` file (`ContextFactory::State`, `SensitivityEvent::Subscribers`).
- Avoid using inline definitions in headers, define functions/methods in their assosciated .cpp file. This is to improve compile times during development and to help maintain ABI stability when the project settles. Class templates with a fixed set of types (`Signal<T>`, `Input<T>`, `Output<T>`, `DerivedSignal<T>`) are defined in the `.cpp` file and explicitly instantiated there.
- What stays inline in a header: templates over a user's type, `constexpr` functions, and the one-line accessors on the simulation run cycle. Say so in a comment next to the definition. There are two groups:
  - What model code calls on every evaluation: `Signal::read()`, `Input::read()`, `Output::write()`, the `pos()`/`neg()`/`change()` event accessors, `Model::context()`, `Module::inputs()`/`outputs()`, the awaitables' `await_ready()`/`await_resume()`, `Task`. Model code is compiled outside of `dspsim-core`, so the compiler only sees what is in the headers.
  - What the scheduler reads on every evaluation: the `ProcessBase` flags and counters, `TimeEvent`'s accessors and comparisons, `Context::current_process()`/`update_count()`, the `scheduled_flag()` functions, `SignalBase::width()`, `BitPart::width()`.
- The core library is a unity build (all core `.cpp` files compiled as one translation unit, see "Build configuration of the core library" in [Architecture.md](Architecture.md)). File-local names (`static` functions, anonymous namespaces) must be unique across the core `.cpp` files, and a `using namespace` at file scope leaks into the files after it.
- Log through the `DSPSIM_LOG_*` macros in `src/log.h`. Do not include spdlog anywhere but `src/log.cpp`, and build messages with string concatenation rather than `std::format`: both cost seconds of compile time per source file.
- In hot-path code, do not pass the address of a local (or a reference to one) to a function that might not be inlined, `std::vector::push_back()` included: the local then lives on the stack for every call. See `FlaggedStack`.
- Check the performance of a hot-path or build change with instruction counts (`perf stat -e instructions:u`) for both a C++ benchmark and a simulation driven from Python. The benchmarks are executables and the Python extensions are shared libraries, and the same code can compile differently for the two.
- While the project is still in development, ABI stability is not a priority. I will remove this line when ABI stability is important.
- Add tests for all new features.
- The simulator in this project has two major phases. construction/elaboration, and simulation run cycle. Code that exists in the "hot-path" simulation run cycle should be prioritized for performance over all else. Outside of the code path, performance is not a concern and easy, intuitive interfaces should be a priority.

### Documentation
- Add doxygen style docstrings for all declarations in the header.
- Update as things change.
