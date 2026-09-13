
verilate(library
SOURCES /home/frank/dspsim/src/dspsim/hdl/HellModel.sv
VERILATOR_ARGS --quiet -F ${CMAKE_BINARY_DIR}/library/HellModel_verilator_args.txt)

verilate(library
SOURCES /home/frank/dspsim/src/dspsim/hdl/Skid.sv
VERILATOR_ARGS --quiet -F ${CMAKE_BINARY_DIR}/library/Skid_verilator_args.txt)

verilate(library
SOURCES /home/frank/dspsim/src/dspsim/hdl/SimpleModel.sv
VERILATOR_ARGS --quiet -F ${CMAKE_BINARY_DIR}/library/SimpleModel_verilator_args.txt)

verilate(library
SOURCES /home/frank/dspsim/src/dspsim/hdl/SimpleModel.sv
VERILATOR_ARGS --quiet -F ${CMAKE_BINARY_DIR}/library/SimpleModel16_verilator_args.txt)

verilate(library
SOURCES /home/frank/dspsim/src/dspsim/hdl/SimpleModel.sv
VERILATOR_ARGS --quiet -F ${CMAKE_BINARY_DIR}/library/SimpleModel8_verilator_args.txt)
