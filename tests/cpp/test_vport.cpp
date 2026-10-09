#include <dspsim/vmodule/vport.h>

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using namespace dspsim::detail;

TEST_CASE("vport storage types", "[vport]")
{
    STATIC_REQUIRE(std::is_same_v<vl_storage_t<int8_t>, uint8_t>);
    STATIC_REQUIRE(std::is_same_v<vl_storage_t<int64_t>, uint64_t>);
    STATIC_REQUIRE(std::is_same_v<vl_storage_t<uint16_t>, uint16_t>);
    STATIC_REQUIRE(std::is_same_v<vl_storage_t<double>, double>);
}

TEST_CASE("vport sign extension", "[vport]")
{
    // 5-bit signed values stored in a CData.
    STATIC_REQUIRE(vl_extend<int8_t, 5>(0x1D) == -3);
    STATIC_REQUIRE(vl_extend<int8_t, 5>(0x10) == -16);
    STATIC_REQUIRE(vl_extend<int8_t, 5>(0x0F) == 15);
    STATIC_REQUIRE(vl_extend<int8_t, 5>(0x00) == 0);

    // 1-bit signed: the only values are 0 and -1.
    STATIC_REQUIRE(vl_extend<int8_t, 1>(1) == -1);
    STATIC_REQUIRE(vl_extend<int8_t, 1>(0) == 0);

    // 24-bit signed in an IData, 33-bit signed in a QData.
    STATIC_REQUIRE(vl_extend<int32_t, 24>(0xFFFFFFu) == -1);
    STATIC_REQUIRE(vl_extend<int32_t, 24>(0x800000u) == -8388608);
    STATIC_REQUIRE(vl_extend<int32_t, 24>(0x7FFFFFu) == 8388607);
    STATIC_REQUIRE(vl_extend<int64_t, 33>(0x1FFFFFFFFull) == -1);

    // Full width needs no extension.
    STATIC_REQUIRE(vl_extend<int16_t, 16>(0x8000) == INT16_MIN);
    STATIC_REQUIRE(vl_extend<int64_t, 64>(~0ull) == -1);

    // Unsigned types pass through unchanged.
    STATIC_REQUIRE(vl_extend<uint8_t, 5>(0x1D) == 0x1D);
}

TEST_CASE("vport input truncation", "[vport]")
{
    // Signed values are written two's complement and masked to the port width.
    STATIC_REQUIRE(vl_truncate<int8_t, 5>(-3) == 0x1D);
    STATIC_REQUIRE(vl_truncate<int8_t, 1>(-1) == 1);
    STATIC_REQUIRE(vl_truncate<int32_t, 24>(-1) == 0xFFFFFFu);
    STATIC_REQUIRE(vl_truncate<int64_t, 33>(-1) == 0x1FFFFFFFFull);
    STATIC_REQUIRE(vl_truncate<int64_t, 64>(-1) == ~0ull);

    // Unsigned inputs are masked too, so the model never sees dirty upper bits.
    STATIC_REQUIRE(vl_truncate<uint8_t, 5>(0xFF) == 0x1F);
    STATIC_REQUIRE(vl_truncate<uint32_t, 24>(0x1000005u) == 5);
    STATIC_REQUIRE(vl_truncate<uint32_t, 32>(0xFFFFFFFFu) == 0xFFFFFFFFu);

    // Out-of-range signed values wrap within the port width.
    STATIC_REQUIRE(vl_extend<int8_t, 5>(vl_truncate<int8_t, 5>(20)) == -12);
    STATIC_REQUIRE(vl_extend<int32_t, 24>(vl_truncate<int32_t, 24>(8388608)) == -8388608);

    // Real ports pass through.
    STATIC_REQUIRE(vl_truncate<double, 64>(1.5) == 1.5);
}
