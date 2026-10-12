"""An example library of dspsim models: a verilated model and a C++ module in one extension module."""

import dspsim
from some_example import library

# The extension module shares the context factory of the framework.
dspsim.link_module(library)
