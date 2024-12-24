from dspsim.framework import Context


def test_context_basic():
    with Context(1e-9, 1e-9) as context:
        print(context)
