"""Test basic package configuration."""

import dspsim
import dspsim.framework
import dspsim.library


def test_exts():
    assert dspsim.framework.foo42() == 42
    assert dspsim.library.foo42() == 42
