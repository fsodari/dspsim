from dspsim import framework


def test_basics():
    print(framework.hello_from_bin())
    print(framework.foo42())


if __name__ == "__main__":
    test_basics()
