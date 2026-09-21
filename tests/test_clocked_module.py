from dspsim.framework import Clock, Context, Input8, Module, Output32, Signal32


class Counter(Module):
    clk: Input8
    count: Output32

    def __init__(self, name: str):
        super().__init__(name)

        self.clk = Input8("clk")
        self.count = Output32("count")

        self.register_process(self.eval, "Counter.eval")
        self.always(self.clk.pos())

        # Skip the initial eval step
        self.initialize = False

    def eval(self):
        if self.clk.posedge():
            self.count.d = self.count.q + 1
            print(f"Counter updated: {self.count.d}")
        else:
            assert 0, "eval called on non-positive edge"


def test_counter():
    with Context() as ctx:
        with ctx.construct():
            clk = Clock("clk", 10)
            count = Signal32("count")

            counter = Counter("counter")

            counter.clk(clk)
            counter.count.bind(count)

        assert counter.count.q == 0
        ctx.run(10)
        assert counter.count.q == 1
        assert counter.count.q == 11
