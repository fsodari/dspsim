import threading
import time

from dspsim.framework import Context, Model, Simulator


class SomeModel(Model):
    def __init__(self):
        # Must call!
        super().__init__()
        self.eval_step_called = 0
        self.eval_end_step_called = 0

    def eval_step(self):
        self.eval_step_called += 1

    def eval_end_step(self):
        self.eval_end_step_called += 1


def test_model_initialization():
    with Context() as context:
        with context.construct():
            some_model = SomeModel()
            assert some_model is not None
            assert isinstance(some_model, Model)
            assert some_model.context is not None
            print(some_model)

            amodel = SomeModel()
            assert amodel is not None
            assert isinstance(amodel, Model)
            print(amodel)

            # Ensure the model's context is the same as the global
            assert some_model.context.id == context.id
            assert (
                len(context.models) == 2
            )  # Ensure the model is registered in the context

        # Create simulator
        sim = Simulator(context)

        N = 5
        for _ in range(N):
            sim.eval()
        assert some_model.eval_step_called == N
        assert some_model.eval_end_step_called == N
        assert amodel.eval_step_called == N
        assert amodel.eval_end_step_called == N

        print(context)
        print(context.models)


def test_multithreaded_models():
    def s1():
        with Context() as context:
            with context.construct():
                models = [SomeModel() for _ in range(30)]
            sim = Simulator(context)
            N = 100
            for _ in range(N):
                sim.eval()
                time.sleep(0.01)
            for model in models:
                assert model.eval_step_called == N
                assert model.eval_end_step_called == N

    threads = [threading.Thread(target=s1) for _ in range(20)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def test_cleanup():
    ctx = Context()
    a = SomeModel()
    SomeModel()
    sim = Simulator(ctx)
    sim.run(10)
    # No need to explicitly clear() the context.
