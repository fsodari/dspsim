import threading
import time

from dspsim.framework import Context, Module


class SomeModel(Module):
    def __init__(self, name: str):
        # Must call!
        super().__init__(name)
        self.eval_called = 0

    def eval(self):
        self.eval_called += 1
        # print(f"SomeModel eval(), {self.eval_called}")


def test_model_initialization():
    with Context() as context:
        with context.construct():
            some_model = SomeModel("some_model")
            assert some_model is not None
            assert isinstance(some_model, Module)
            assert some_model.context is not None
            # print(some_model)

            amodel = SomeModel("amodel")
            assert amodel is not None
            assert isinstance(amodel, Module)
            # print(amodel)

            # Ensure the model's context is the same as the global
            assert some_model.context.id == context.id
            assert (
                len(context.models) == 2
            )  # Ensure the model is registered in the context

        # Create simulator
        N = 5
        for _ in range(N):
            context.eval()

        # Module won't be automatically run in a delta cycle. Just once at initialization.
        # Need to trigger it to eval somehow.
        assert some_model.eval_called == 1
        assert amodel.eval_called == 1


def test_multithreaded_models():
    def s1():
        with Context().obtain_lock() as context:
            with context.construct():
                models = [SomeModel(f"some_model_{i}") for i in range(30)]
            N = 20
            for _ in range(N):
                context.eval()
                time.sleep(0.003)
            for model in models:
                assert model.eval_called == 1

    threads = [threading.Thread(target=s1) for _ in range(20)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def test_cleanup():
    ctx = Context()
    a = SomeModel("a")
    SomeModel("b")
    for _ in range(10):
        ctx.eval()
    # No need to explicitly clear() the context.
