from dspsim.framework import Context, Input32, Module, ModuleName, Output32, Signal32


class AdderModule(Module):
    a: Input32
    b: Input32
    c: Output32

    def __init__(self, name: str):
        _name = ModuleName(name)
        super().__init__(_name)

        self.a = Input32("a")
        self.b = Input32("b")
        self.c = Output32("c")

        self.always(self.a, self.b)
        # self.always.add_event(self.a.change())
        # self.always.add_event(self.b.change())
        # self.initialize = False

    def eval(self):
        print("AdderModule eval called with a =", self.a.q, "b =", self.b.q)
        self.c.write(self.a.q + self.b.q)


def test_adder_module():
    with Context() as ctx:
        with ctx.construct():
            # ctx.log_level = "trace"
            a = Signal32("a")
            b = Signal32("b")
            c = Signal32("c")

            adder = AdderModule("adder")
            print("Binding signals to adder module")
            adder.a.bind(a)
            adder.b.bind(b)
            adder.c.bind(c)

        # Set input values and evaluate the adder
        a.d = 3
        b.d = 5
        ctx.eval()
        print(c.q)
        assert c.q == 8

        a.d = 4
        b.d = 6
        ctx.run(10)
        print(c.q)
        assert c.q == 10
