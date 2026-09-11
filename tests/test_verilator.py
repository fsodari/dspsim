from dspsim import verilator
from pathlib import Path


def test_verilator_root():
    root = verilator.verilator_root()
    assert root.exists()
    print(root)


def test_verilator_bin():
    bin_path = verilator.verilator_bin()
    assert bin_path.exists()
    print(bin_path)


def test_verilator_version():
    result = verilator.verilator(["--version"], capture_output=True, check=True)
    assert result.returncode == 0
    print(result.stdout.decode())


def test_verilator_json():
    src_file = Path("src/dspsim/hdl/SimpleModel.sv")
    result = verilator.verilate_json(sources=[src_file])
    print(result)
