import pytest

from dspsim.framework import (
    Context,
    Input8,
    Input8Array,
    Module,
    Output8,
    Output8Array,
    Signal8,
    Signal8Array,
)


def test_signal_array():
    with Context() as ctx:
        s = Signal8Array("s", (2, 3, 4), init=1)
        assert s.shape == (2, 3, 4)
        assert s.ndim == 3
        assert len(s) == 24
        assert s.extent(1) == 3
        assert isinstance(s[1, 2, 3], Signal8)
        assert s[1, 2, 3].name == "s[1][2][3]"
        assert s[(1, 2, 3)] is s.flat(23)

        ctx.elaborate()

        s[0, 1, 2] = 7
        assert s[0, 1, 2].d == 7

        ctx.run(1)
        assert s[0, 1, 2].value == 7
        assert s[0, 0, 0].value == 1
        with pytest.raises(IndexError):
            s[2, 0, 0]
        with pytest.raises(IndexError):
            s[0, 0]


def test_port_array_binding():
    with Context() as ctx:
        s = Signal8Array("s", (2, 2))
        t = Signal8Array("t", (3,))

        class Mod(Module):
            def __init__(self, name):
                self.i = Input8Array("i", (2, 2))
                self.o = Output8Array("o", (2, 2))

        m = Mod("m")
        m.i.bind(s)
        m.o(s)
        with pytest.raises(Exception):  # noqa: B017
            m.i.bind(t)
        with pytest.raises(TypeError):
            m.i.bind(m.o)  # type: ignore

        ctx.elaborate()
        m.o[1, 1] = 5

        ctx.run(1)
        assert m.i[1, 1].value == 5
