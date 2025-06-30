"""
Test the verilator utilities in the dspsim package.
"""

from dspsim.vmodel_info import ModuleConfig
from pathlib import Path
from dspsim.util import render_template


def test_model_gen():
    """"""
    source = Path("src/dspsim/hdl/SomeModel.sv")

    model = ModuleConfig.load_model(source, parameters={}, verilator_args=[])

    model_gen = render_template("model.cpp.jinja", model=model, trace="vcd")
    print(model_gen)
