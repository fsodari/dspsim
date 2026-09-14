import time
from pathlib import Path

from dspsim.framework.verilator import parse_module_json, verilate_json


def test_verilate_json():
    print()
    hdl_dir = Path(__file__).parent / "test_modules"
    # Example test for verilate_json function
    # hdl_file = Path(__file__).parent / "test_modules" / "HellModel.sv"
    hdl_file = hdl_dir / "HellModel.sv"
    odir = Path("build")
    json_output, metadata = verilate_json(
        [hdl_file],
        output_dir=odir,
        include_dirs=[hdl_dir],
    )

    start_time = time.time()
    module_info = parse_module_json(json_output, metadata)
    elapsed_time = time.time() - start_time
    print(f"Parsing module JSON took {elapsed_time:.6f} seconds")

    print(module_info)
    for parameter in module_info.parameters.values():
        print(parameter)
    for port in module_info.ports.values():
        print(port)
