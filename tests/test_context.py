from dspsim.framework import Context, Clock, Signal8, Dff8, Signal32, Dff32


def test_context_basic():
    with Context(1e-9, 1e-9) as context:
        print(context.print_info())
