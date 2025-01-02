"""
Test the com port talking to the demo USBBridge device.
Requires hardware, so cannot be run unless the appropriate hardware is connected.
"""

import random
from tqdm import tqdm
import numpy as np

import string
from pathlib import Path
from dspsim.avril import Avril, VIFace, VReg, ErrorCode, AvrilMode
import pytest


# @pytest.mark.skip
def test_avril():
    """"""
    with Avril(AvrilMode.Vmmi, timeout=0.02) as av:
        print(av)
        # meta = av.read_all_meta()
        # # print(meta)
        # for name, iface in meta.items():
        #     print(f"{name}: {iface}")

        sram0 = av.get_interface("sram0")
        print(sram0)
        sram0.load_register_file(Path("reg_map.yaml"))

        sram0["x"] = 99
        assert sram0["x"] == 99

        sram1 = VIFace(av, "sram1")

        ack = sram1.write_reg(0, 42)
        assert ack.error == ErrorCode.NoError
        print(ack)

        ack = sram1.read_reg(0)
        assert ack.error == ErrorCode.NoError
        print(ack, ack.data)

        # assign random register names to all registers.
        sram1.load_registers(
            {
                "".join(random.choices(string.ascii_lowercase, k=32)): VReg(i)
                for i in sram1
            }
        )

        # Iterate through all addresses, incrementing by dtype.
        for addr in tqdm(sram0):
            x = random.randint(-(2**31), 2**31 - 1)
            sram0[addr] = x
            y = sram0[addr]
            assert y == x

        # Iterate through all registers.
        for r in tqdm(sram1.registers):
            x = random.random()
            sram1[r] = x
            y = sram1[r]
            assert np.isclose(y, x, atol=0.00001)

        # Write out of bounds.
        try:
            ack = sram1.write_reg(1024, 42)
            assert False
        except Exception as e:
            pass
        try:
            # Read out of bounds
            ack = sram1.read_reg(1024)
            assert False
        except Exception as e:
            pass

        # getitem/setitem raises an exception if there is an ack error.
        try:
            ack = sram1[1024]
            assert False
        except Exception as e:
            print(e)

        # Multiple registers
        sram1[0] = np.random.random((10,))
        ack = sram1.read_reg(0, 10)
        for a in ack:
            print(a)
        # print(*ack)


def test_avril_dict():
    """"""
    with Avril(AvrilMode.Vmmi, timeout=0.02) as av:
        cdict = av.get_interface("dict")
        print(cdict)

        # Check current state.
        current_state = {}
        for a in cdict:
            ack = cdict.read_reg(a)
            if ack.error == ErrorCode.NoError:
                current_state[a] = ack.data

        print("Current State")
        for k, v in current_state.items():
            print(f"{k}:{v}")

        # Random addresses and random data.
        N = 3
        test_data = {random.randrange(0, 1024, 4): random.random() for _ in range(N)}

        # Write the test data.
        print("Writing dict")
        for k, v in test_data.items():
            print(f"{k}:{v}")
            cdict[k] = v

        # Read back
        print("Readback")
        readback = {k: cdict[k] for k in test_data}
        for k, v in readback.items():
            print(f"{k}:{v}")

        assert np.all(
            np.isclose(list(readback.values()), list(test_data.values()), atol=0.001)
        )
        # cdict[0] = 1.2
        # print(cdict[0])
