from pathlib import Path

import verilator

import dspsim
from dspsim import verilate


def test_verilator_root():
    root = verilator.verilator_root()
    assert root.exists()
    print(root)


def test_verilator_bin():
    bin_path = verilator.verilator_bin()
    assert bin_path.exists()
    print(bin_path)


def test_verilator_version():
    result = verilator.verilator(["--version"], capture_output=True)
    assert result.returncode == 0
    print(result.stdout.decode())


def test_verilator_json():
    src_file = dspsim.hdl_dir() / "SimpleModel.sv"
    result = verilate.verilate_json(sources=[src_file])
    print(result)
