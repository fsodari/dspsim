from dspsim.framework import (
    Clock,
    Context,
    InputU8,
    Module,
    OutputU32,
    SignalU8,
    SignalU32,
)


class Counter(Module):
    clk: InputU8
    rst: InputU8
    count: OutputU32

    def __init__(self, name: str):
        super().__init__(name)

        self.clk = InputU8("clk")
        self.rst = InputU8("rst")
        self.count = OutputU32("count")

        self.process(self.eval, "Counter.eval").always(self.clk.pos()).initialize(False)

    def eval(self):
        if self.clk.pos():
            self.count.d = self.count.q + 1
            print(f"Counter updated: {self.count.d}")
        else:
            assert 0, "eval called on non-positive edge"

        if self.rst.q:
            self.count.d = 0


def test_counter():
    with Context() as ctx:
        with ctx.construct():
            clk = Clock("clk", 10)
            rst = SignalU8("rst")
            count = SignalU32("count")

            counter = Counter("counter")

            counter.clk.bind(clk)
            counter.rst.bind(rst)
            counter.count.bind(count)

        assert counter.count.q == 0

        rst.d = 1
        ctx.run(20)
        assert counter.count.q == 0

        rst.d = 0
        ctx.run(100)
        assert counter.count.q == 10
