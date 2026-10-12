# DSPSim

dspsim is a C++ discrete-event (delta-cycle) simulation engine, exposed to Python via
nanobind, used to simulate hardware designs, either hand-written C++ models or
Verilator-generated models of Verilog/SystemVerilog RTL. Synthesizable VHDL is supported
as well, through [GHDL](https://github.com/ghdl/ghdl) ([docs/Vhdl.md](docs/Vhdl.md)).
