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

### Class organization
- From top to bottom, declare members in this order: types/enums, consts/constexprs, constructors/destructors, static factory functions, public methods, protected methods, private methods, public member variables, protected member variables, private member variables.
- Member variables must be declared in the order they are initialized, specifically in the order they must be initialized
- In this project specifically, if an object obtains or is passed a Context*, that should be the first member.
- Group related functions together.
- Definitions in the cpp file should match the order they were declared in the class.

### Project conventions
- There are a number of classes that reference each other. Use forward declarations when needed and don't include the declared class's header.
- Avoid using inline definitions in headers, define functions/methods in their assosciated .cpp file. This is to improve compile times during development and to help maintain ABI stability when the project settles.
- While the project is still in development, ABI stability is not a priority. I will remove this line when ABI stability is important.
- Add tests for all new features.
- The simulator in this project has two major phases. construction/elaboration, and simulation run cycle. Code that exists in the "hot-path" simulation run cycle should be prioritized for performance over all else. Outside of the code path, performance is not a concern and easy, intuitive interfaces should be a priority.

### Documentation
- Add doxygen style docstrings for all declarations in the header.
- Update as things change.
