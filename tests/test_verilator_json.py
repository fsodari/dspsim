import time
from pathlib import Path

from dspsim.verilator import parse_module_json, verilate_json

HDL_DIR = Path(__file__).parent.parent / "hdl"


def test_verilate_json():
    print()
    # Example test for verilate_json function
    source = HDL_DIR / "HellModel.sv"
    odir = Path("build")
    json_output, metadata = verilate_json(
        [source],
        output_dir=odir,
        include_dirs=[HDL_DIR],
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
