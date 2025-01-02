#include <catch2/catch_test_macros.hpp>

extern "C"
{
#include "dspsim/psoc/cdict.h"
#include "dspsim/psoc/error_codes.h"
}
#include <iostream>

TEST_CASE("cdict_basic")
{
    Dict dict = dict_createl(8);

    for (uint32_t i = 0; i < 10; i++)
    {
        uint32_t test_data = hash_bytes_func(NULL, &i, sizeof(uint32_t));
        // std::cout << "i: " << i << ", value: " << test_data % dict->n_bins << std::endl;
        uint32_t error = dict_setl(dict, i, &test_data, sizeof(test_data));
        REQUIRE(error == dErrNone);
        REQUIRE(dict_size(dict) == (i + 1));

        uint32_t readback = -99;
        uint32_t copied = dict_get(dict, &i, &readback);
        REQUIRE(copied == sizeof(readback));
        REQUIRE(readback == test_data);
    }

    for (uint32_t i = 0; i < 10; i++)
    {
        uint32_t error = dict_setl(dict, i, &i, sizeof(i));
        REQUIRE(error == dErrNone);

        // Same keys so the dict won't increase in size.
        REQUIRE(dict_size(dict) == 10);

        uint32_t readback = -99;
        uint32_t copied = dict_get(dict, &i, &readback);
        REQUIRE(copied == sizeof(readback));
        REQUIRE(readback == i);
    }
}

TEST_CASE("cdict_str")
{
    Dict dict = dict_create_str(8, dstr16);

    const char *x = "SomeValue";
    auto error = dict_set(dict, "foo", x, 16);
    REQUIRE(error == dErrNone);

    const char *readback = dict_ref_str(dict, "foo");
    std::cout << readback << std::endl;
    REQUIRE(strcmp(x, readback) == 0);
}
