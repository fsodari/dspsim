"""
Test the verilator utilities in the dspsim package.
"""

from dspsim.config import Parameter, Port, ModuleConfig
from dspsim.verilator import VModelTree

from pathlib import Path

from dspsim.util import render_template


def test_module_json():
    """"""
    source = Path("src/dspsim/hdl/SomeModel.sv")

    model = ModuleConfig.from_verilator(source, parameters={}, verilator_args=[])
    # print(model.name)
    # print(model.source)
    # for name, param in model.parameters.items():
    #     print(param.sv_mod_def)

    # for name, port in model.ports.items():
    #     print(port.sv_mod_def)

    # for param in model.parameters.values():
    #     print(param.vm_def)

    # for port in model.ports.values():
    #     print(port.vm_port_decl)
    #     print(port.vm_ctor_arg)
    #     print(port.vm_ctor_init)

    print()
    model_gen = render_template("model.cpp.jinja", model=model, trace="vcd")
    print(model_gen)
    # print(model.ports)
    # tree = VModelTree.generate(source, parameters={}, verilator_args=[])
    # print(tree)
    # print("Tree:")
    # print(f"name: {tree.name}")
    # print(f"source: {tree.source}")
    # print(f"parameters: {tree.parameters}")
    # print(f"ports: {tree.ports}")
    # # print("Meta:")
    # # print(tree._meta_content["files"])
