import os
from pathlib import Path

from dspsim import verilator

HDL_DIR = Path(__file__).parent.parent / "hdl"


def test_verilator_root():
    root = verilator.verilator_root()
    assert root.exists()

    # Ensure VERILATOR_ROOT exists in env and matches verilator_root()
    assert verilator.verilator_root() == Path(os.environ["VERILATOR_ROOT"])


def test_verilator_bin():
    bin_path = verilator.verilator_bin()
    assert bin_path.exists()


def test_verilator_version():
    result = verilator.verilator(["--version"], capture_output=True)
    assert result.returncode == 0


def test_verilator_json():

    src_file = HDL_DIR / "SimpleModel.sv"
    model_data, metadata = verilator.verilate_json(
        sources=[src_file], include_dirs=[HDL_DIR]
    )

    # Check that the top-level module name is correct.
    name = model_data["modulesp"][0]["name"]
    assert name == "SimpleModel"

    # Check that the metadata contains the expected source file.
    assert src_file == Path(metadata["files"]["e"]["filename"])
