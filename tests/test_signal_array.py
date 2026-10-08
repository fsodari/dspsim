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
            s[0, 0, 0, 0]


def test_port_array_binding():
    with Context() as ctx:
        s = Signal8Array("s", (2, 2))
        t = Signal8Array("t", (3,))

        class Mod(Module):
            def __init__(self, name):
                super().__init__(name)
                self.i = Input8Array("i", (2, 2))
                self.o = Output8Array("o", (2, 2))

                self.process(self.eval).always("*")

            def eval(self):
                for i in range(self.i.size):
                    self.o.flat(i).d = self.i.flat(i).q

        m = Mod("m")
        m.i.bind(s)
        m.o(s)
        with pytest.raises(Exception):  # noqa: B017
            m.i.bind(t)
        with pytest.raises(TypeError):
            m.i.bind(m.o)  # type: ignore

        ctx.elaborate()

        for i in range(s.size):
            s.flat(i).d = i

        ctx.run(1)
        for i in range(s.size):
            assert m.o.flat(i).value == i


def test_array_iter():
    with Context():
        s = Signal8Array("s", (2, 3))
        elems = list(s)
        assert len(elems) == 6
        assert all(isinstance(e, Signal8) for e in elems)
        assert elems[4] is s.flat(4)
        assert [e.name for e in s][1] == "s[0][1]"

        class Holder(Module):
            def __init__(self, name):
                super().__init__(name)
                self.i = Input8Array("i", (2, 2))

        assert len(list(Holder("h").i)) == 4


def test_array_slicing():
    from dspsim.framework import OutputArrayView8, SignalArrayView8

    with Context() as ctx:
        s = Signal8Array("s", (4, 6))
        v = s[1:4, ::2]
        assert isinstance(v, SignalArrayView8)
        assert v.shape == (3, 3)
        assert v[0, 0] is s[1, 0]
        assert v[2, 2] is s[3, 4]
        assert s[-1, ::-1].shape == (6,)
        assert s[-1, ::-1][0] is s[3, 5]
        assert s[2, :].shape == (6,)
        assert s[:, 1].shape == (4,)
        assert s[3:1].shape == (0, 6)
        assert [e.name for e in s[0, :2]] == ["s[0][0]", "s[0][1]"]
        assert v[1:, 1:][0, 0] is s[2, 2]
        # slice() always returns a view, even for int keys.
        assert s.slice((slice(1, 4), slice(None, None, 2))).shape == (3, 3)
        assert s.slice((1, 2)).shape == ()
        assert s.slice(2).shape == (6,)
        assert s.slice().shape == (4, 6)
        assert s.slice(()).size == 24
        assert s.slice((slice(None), 1))[2] is s[2, 1]
        with pytest.raises(IndexError):
            s[4]
        with pytest.raises(IndexError):
            s[0, 0, 0]

        class M(Module):
            def __init__(self, name):
                super().__init__(name)
                self.i = Input8Array("i", (2, 3))
                self.o = Output8Array("o", (2, 3))

        m = M("m")
        i, o = m.i, m.o
        a = Signal8Array("a", (4, 6))
        b = Signal8Array("b", (4, 6))
        i.bind(a[::2, ::2])
        o[0:1].bind(b[3:4, 0:3])
        assert isinstance(o[0:1], OutputArrayView8)
        with pytest.raises(Exception):
            i.bind(a[:2])
        with pytest.raises(TypeError):
            i.bind(o[:])  # type: ignore
        o[1, :].bind(b[0, :3])

        ctx.elaborate()
        a[2, 4].d = 9
        ctx.run(1)
        assert i[1, 2].value == 9
