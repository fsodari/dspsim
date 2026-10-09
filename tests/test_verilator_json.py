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


def test_signed_ports():
    """Signed HDL ports are detected and select the signed vport macros."""
    json_output, metadata = verilate_json(
        [HDL_DIR / "HellModel.sv"], include_dirs=[HDL_DIR]
    )
    ports = parse_module_json(json_output, metadata).ports

    expected = {
        "a": (False, "DSPSIM_VINPUT"),
        "b": (False, "DSPSIM_VOUTPUT"),
        "c": (True, "DSPSIM_VINPUT_S"),
        "d": (True, "DSPSIM_VOUTPUT_S"),
        "e": (True, "DSPSIM_VOUTPUT_ARRAY_S"),
        "f": (True, "DSPSIM_VOUTPUT_ARRAY_S"),
        "g": (False, "DSPSIM_VOUTPUT_ARRAY"),
    }
    for name, (signed, macro) in expected.items():
        assert ports[name].signed == signed, name
        assert ports[name].vmacro == macro, name
