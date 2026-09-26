from dspsim.framework import Context, Input32, Module, Output32, Signal32


class SubAdder(Module):
    a: Input32
    b: Input32
    c: Output32

    def __init__(self, name: str):
        super().__init__(name)

        self.a = Input32("a")
        self.b = Input32("b")
        self.c = Output32("c")

        self.process(self.eval, "SubAdder.eval").always("*")

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
        self.sub.a(self.a)
        self.sub.b(self.b)
        self.sub.c(self.c)


def test_adder_module():
    with Context() as ctx:
        with ctx.construct():
            a = Signal32("a")
            b = Signal32("b")
            c = Signal32("c")

            d = Signal32("d")
            e = Signal32("e")
            f = Signal32("f")

            adder = AdderModule("adder")
            adder2 = AdderModule("adder2")

            adder.a(a)
            adder.b(b)
            adder.c(c)
            adder2.a.bind(d)
            adder2.b.bind(e)
            adder2.c.bind(f)

        # Set input values and evaluate the adders
        a.d = 3
        b.d = 5
        d.d = 7
        e.d = 9
        ctx.eval()
        assert c.q == 8
        assert f.q == 16

        a.d = 4
        b.d = 6
        d.d = 10
        e.d = 12
        ctx.run(10)
        assert c.q == 10
        assert f.q == 22

        # ctx.print_hierarchy()


class TwoAdder(Module):
    """Adder with two adder processes"""

    a: Input32
    b: Input32
    c: Output32

    d: Input32
    e: Input32
    f: Output32

    def __init__(self, name: str):
        super().__init__(name)

        self.a = Input32("a")
        self.b = Input32("b")
        self.c = Output32("c")

        self.d = Input32("d")
        self.e = Input32("e")
        self.f = Output32("f")

        self.process(self.eval, "SubAdder.eval").always(self.a, self.b)

        self.process(self.eval2, "SubAdder.eval2").always(self.d, self.e)

    def eval(self):
        self.c.d = self.a.q + self.b.q

    def eval2(self):
        self.f.d = self.d.q + self.e.q


def test_two_adder():
    with Context() as ctx:
        # ctx.log_level = "debug"
        with ctx.construct():
            a = Signal32("a")
            b = Signal32("b")
            c = Signal32("c")

            d = Signal32("d")
            e = Signal32("e")
            f = Signal32("f")

            two_adder = TwoAdder("two_adder")
            two_adder.a(a)
            two_adder.b(b)
            two_adder.c(c)
            two_adder.d(d)
            two_adder.e(e)
            two_adder.f(f)

        # Set input values and evaluate the two_adder
        a.d = 1
        b.d = 2
        d.d = 3
        e.d = 4
        ctx.eval()
        assert c.q == 3
        assert f.q == 7

        a.d = 5
        b.d = 6
        d.d = 7
        e.d = 8
        ctx.run(10)
        assert c.q == 11
        assert f.q == 15
