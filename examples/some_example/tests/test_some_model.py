from some_example.library import MovingAverage, SomeModel

from dspsim.framework import Clock, Context, SignalS32, SignalU8, SignalU32


def test_some_model():
    """The verilated model. The global parameter DW = 24 applies to it."""
    assert SomeModel.DW == 24

    with Context() as ctx:
        clk = Clock("clk", 10)
        rst = SignalU8("rst", 1)
        i = SignalU32("i", SomeModel.DW)
        o = SignalU32("o", SomeModel.DW)

        some_model = SomeModel("some_model")
        some_model.clk.bind(clk)
        some_model.rst.bind(rst)
        some_model.i.bind(i)
        some_model.o.bind(o)

        ctx.elaborate()

        rst.d = 1
        ctx.run(100)
        rst.d = 0
        ctx.run(10)
        for x in range(10):
            i.d = x
            ctx.run(10)
            assert o.q == x


def test_moving_average():
    """The C++ module, with its constructor argument and its methods."""
    with Context() as ctx:
        clk = Clock("clk", 10)
        d = SignalS32("d")
        q = SignalS32("q")

        average = MovingAverage("average", length=2)
        assert average.length() == 2
        assert MovingAverage("default").length() == 4
        average.clk.bind(clk)
        average.d.bind(d)
        average.q.bind(q)

        ctx.elaborate()

        d.d = 10
        ctx.run(25)
        assert q.q == 10

        d.d = 20
        ctx.run(10)
        assert q.q == 15
