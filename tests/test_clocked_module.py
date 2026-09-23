from dspsim.framework import Clock, Context, Input8, Module, Output32, Signal8, Signal32


class Counter(Module):
    clk: Input8
    rst: Input8
    count: Output32

    def __init__(self, name: str):
        super().__init__(name)

        self.clk = Input8("clk")
        self.rst = Input8("rst")
        self.count = Output32("count")

        self.process(self.eval, "Counter.eval").always(self.clk.pos())

        # Skip the initial eval step
        self.initialize = False

    def eval(self):
        if self.clk.posedge():
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
            rst = Signal8("rst")
            count = Signal32("count")

            counter = Counter("counter")

            counter.clk(clk)
            counter.rst(rst)
            counter.count.bind(count)

        assert counter.count.q == 0

        rst.d = 1
        ctx.run(20)
        assert counter.count.q == 0

        rst.d = 0
        ctx.run(100)
        assert counter.count.q == 10
