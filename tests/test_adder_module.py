from dspsim.framework import Context, Input32, Module, ModuleName, Output32, Signal32


class SubAdder(Module):
    a: Input32
    b: Input32
    c: Output32

    def __init__(self, name: str):
        super().__init__(name)

        self.a = Input32("a")
        self.b = Input32("b")
        self.c = Output32("c")

        self.always(self.a.change(), self.b.change())

    def eval(self):
        self.c.d = self.a.q + self.b.q


class AdderModule(Module):
    a: Input32
    b: Input32
    c: Output32

    sub: SubAdder

    def __init__(self, name: str):
        super().__init__(name)

        self.a = Input32("a")
        self.b = Input32("b")
        self.c = Output32("c")

        self.sub = SubAdder("sub")
        self.sub.a.bind(self.a)
        self.sub.b.bind(self.b)
        self.sub.c.bind(self.c)


def test_adder_module():
    with Context() as ctx:
        with ctx.construct():
            ctx.log_level = "trace"
            a = Signal32("a")
            b = Signal32("b")
            c = Signal32("c")

            d = Signal32("d")
            e = Signal32("e")
            f = Signal32("f")

            adder = AdderModule("adder")
            adder2 = AdderModule("adder2")

            adder.a.bind(a)
            adder.b.bind(b)
            adder.c.bind(c)
            adder2.a.bind(d)
            adder2.b.bind(e)
            adder2.c.bind(f)

        # Set input values and evaluate the adders
        a.d = 3
        b.d = 5
        d.d = 7
        e.d = 9
        ctx.eval()
        print(c.q)
        assert c.q == 8
        print(f.q)
        assert f.q == 16

        a.d = 4
        b.d = 6
        d.d = 10
        e.d = 12
        ctx.run(10)
        print(c.q)
        assert c.q == 10
        print(f.q)
        assert f.q == 22

        ctx.print_hierarchy()
