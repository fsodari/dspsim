import gc
import weakref

import pytest

from dspsim.framework import Context, DffU8, InputU8, Module, OutputU8, SignalU8


class Base(Module):
    def __init__(self, name: str, width: int = 8):
        super().__init__(name)
        self.width = width
        self.a = SignalU8("a")


class Derived(Base):
    def __init__(self, name: str, *, extra: int):
        super().__init__(name, width=16)
        self.extra = extra
        self.b = SignalU8("b")


class Derived2(Derived):
    def __init__(self, name: str):
        super().__init__(name, extra=3)
        self.c = SignalU8("c")


class NoOwnInit(Derived2):
    pass


class NoSuper(Module):
    def __init__(self, name: str):
        # super().__init__ is optional, and the context is usable immediately.
        self.ctx_id = self.context.id
        self.i = InputU8("i")


class Parent(Module):
    def __init__(self, name: str):
        self.child = Derived2("child")
        self.after_child = SignalU8("after_child")


def test_sub_subclass_hierarchy():
    with Context() as ctx:
        m = Derived2("m")
        after = SignalU8("after")
        assert m.a.hier_name == "root.m.a"
        assert m.b.hier_name == "root.m.b"
        assert m.c.hier_name == "root.m.c"
        assert after.hier_name == "root.after"
        assert (m.width, m.extra) == (16, 3)
        assert [x.name for x in ctx.modules] == ["m"]


def test_inherited_init():
    with Context():
        m = NoOwnInit("m")
        assert m.c.hier_name == "root.m.c"


def test_nested_modules():
    with Context():
        p = Parent("p")
        assert p.child.c.hier_name == "root.p.child.c"
        assert p.after_child.hier_name == "root.p.after_child"
        assert p.child.parent.name == "p"


def test_no_super_call_and_ownership():
    with Context() as ctx:
        m = NoSuper("m")
        assert m.ctx_id == ctx.id
        assert m.i.hier_name == "root.m.i"
        alive = weakref.ref(m)
        del m
        gc.collect()
        # The context owns the module, so it survives losing the Python reference.
        assert alive() is not None
        assert ctx.modules[0].i.hier_name == "root.m.i"
    # Until the context is cleared, here at the end of the with block.
    gc.collect()
    assert alive() is None


def test_keyword_and_default_name():
    class Named(Module):
        def __init__(self, width: int = 8, name: str = "default_name"):
            self.width = width

    with Context():
        assert Named().name == "default_name"
        assert Named(name="kw", width=4).name == "kw"


def test_failed_init_restores_hierarchy():
    class Bad(Module):
        def __init__(self, name: str):
            SignalU8("inside")
            raise ValueError("boom")

    with Context():
        with pytest.raises(ValueError, match="boom") as excinfo:
            Bad("bad")
        # Keeping the exception (and its frames) alive must not keep the scope open.
        assert excinfo.value is not None
        assert SignalU8("after").hier_name == "root.after"


def test_init_without_name_is_rejected():
    with pytest.raises(TypeError, match="'name' argument"):

        class NoName(Module):
            def __init__(self, width: int):
                pass


def test_ports_of_a_cpp_module():
    """ports(), inputs() and outputs() refer to the ports of the module: they do not take them over."""
    with Context():
        dff = DffU8("dff")
        for _ in range(3):
            ports = dff.ports()
            assert [port.name for port in ports] == ["clk", "d", "q"]
            # The lists can go: the ports stay.
            del ports
            gc.collect()

        assert [type(port) for port in dff.ports()] == [InputU8, InputU8, OutputU8]
        assert dff.inputs()[1] is dff.d
        assert dff.outputs()[0] is dff.q
