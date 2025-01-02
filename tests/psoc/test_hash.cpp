#include <catch2/catch_test_macros.hpp>

extern "C"
{
#include "dspsim/psoc/hash.h"
#include "dspsim/psoc/error_codes.h"
}
#include <iostream>

TEST_CASE("hash_basic")
{
    Hash hash = hash_create(0, 0, 4);
}
