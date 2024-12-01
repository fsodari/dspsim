from dspsim import framework
from dspsim.framework import Context, Clock, Signal8, Dff8, Signal32, Dff32
from dspsim.library import AnotherModel


# @framework.runner(time_unit=1e-9, time_precision=1e-9)
def test_context_basic():
    context = Context(1e-9, 1e-9)

    print(context.print_info())
    clk = Clock(10e-9)
    rst = Dff8(clk, 1)

    x = Dff32(clk, 42)
    y = Signal32(0)

    some_model = AnotherModel(clk, rst, x, y)

    context.elaborate()

    context.advance(100)
    rst.d = 0
    context.advance(100)
    x.d = 99
    context.advance(50)
    assert y.q == 99

    print(context.print_info())


# @framework.runner(1e-9, 1e-12)
# def test_context_2(context: Context):
#     print(context.print_info())


if __name__ == "__main__":
    print("Main Start")
    test_context_basic()
    # test_context_2()
    print("Main Stop")
