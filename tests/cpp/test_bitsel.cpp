#include <dspsim/dspsim.h>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace dspsim;

TEST_CASE("bit helpers", "[bitsel]")
{
    STATIC_REQUIRE(mask(0) == 0);
    STATIC_REQUIRE(mask(8) == 0xFF);
    STATIC_REQUIRE(mask(64) == ~uint64_t{0});
    STATIC_REQUIRE(mask<12>() == 0xFFF);
    STATIC_REQUIRE(mask<8, uint8_t>() == 0xFF);

    STATIC_REQUIRE(bits(0xABCD, 11, 4) == 0xBC);
    STATIC_REQUIRE(bits(0x8000000000000000, 63, 63) == 1);
    STATIC_REQUIRE(zext(0xFFF5, 8) == 0xF5);

    STATIC_REQUIRE(sext(0xFFF5, 16) == -11);
    STATIC_REQUIRE(sext(0x7FF5, 16) == 0x7FF5);
    STATIC_REQUIRE(sext(0x1, 1) == -1);
    STATIC_REQUIRE(sext(0xFFFFFFFFFFFFFFF5, 64) == -11);
    // Bits above the width are ignored.
    STATIC_REQUIRE(sext(0xAB0F, 4) == -1);

    STATIC_REQUIRE(sext<16>(uint32_t{0xFFF5}) == -11);
    STATIC_REQUIRE(sext<5>(uint8_t{0x0F}) == 15);
    STATIC_REQUIRE(sext<5>(uint8_t{0x10}) == -16);
    STATIC_REQUIRE(sext<8>(uint8_t{0x80}) == -128);
    STATIC_REQUIRE(std::is_same_v<decltype(sext<4>(uint16_t{0})), int16_t>);
}

TEST_CASE("slice reads", "[bitsel]")
{
    auto ctx = Context::create();
    Signal<uint16_t> a{"a"};
    a.init(0xA5C3);

    REQUIRE(a.slice(7, 0).read() == 0xC3);
    REQUIRE(a[{15, 8}].read() == 0xA5);
    REQUIRE(a[{11, 4}].read() == 0x5C);
    REQUIRE(a[0].read() == 1);
    REQUIRE(a[2].read() == 0);
    REQUIRE(a[{15, 0}].read() == 0xA5C3);
    REQUIRE(BitSel(a).read() == 0xA5C3);

    auto s = a[{11, 4}];
    REQUIRE(s.width() == 8);
    REQUIRE(s.parts().size() == 1);
    REQUIRE(s.parts()[0] == BitPart{&a, 11, 4});

    // A slice of a slice refers to the signal directly.
    REQUIRE(s[{3, 0}].parts()[0] == BitPart{&a, 7, 4});
    REQUIRE(s[{3, 0}].read() == 0xC);

    // Reads see the committed value, not the pending one.
    ctx->elaborate();
    a.write(0x1234);
    REQUIRE(s.read() == 0x5C);
    ctx->run(0);
    REQUIRE(s.read() == 0x23);
}

TEST_CASE("slices of signed signals are unsigned", "[bitsel]")
{
    auto ctx = Context::create();
    Signal<int32_t> s{"s", 24};
    s.init(-11);

    REQUIRE(s.slice(23, 0).read() == 0xFFFFF5);
    REQUIRE(s[{23, 8}].read() == 0xFFFF);
    REQUIRE(s[{7, 0}].read() == 0xF5);
    REQUIRE(sext(s[{23, 0}].read(), 24) == -11);
    REQUIRE(sext(s[{15, 0}].read(), 16) == -11);
    REQUIRE(s[{15, 0}].read_signed() == -11);
    REQUIRE(s[{7, 0}].read_signed() == -11);
    REQUIRE(s[{7, 4}].read_signed() == -1);
    REQUIRE(s[{3, 0}].read_signed() == 5);
}

TEST_CASE("slice writes", "[bitsel]")
{
    auto ctx = Context::create();
    Signal<uint16_t> a{"a"};
    Signal<int32_t> s{"s", 24};
    ctx->elaborate();

    SECTION("writes to different bits in the same cycle combine")
    {
        a[{15, 8}].write(0xAB);
        a[{7, 0}].write(0xCD);
        REQUIRE(a.read() == 0);
        ctx->run(0);
        REQUIRE(a.read() == 0xABCD);

        // Only the selected bits change.
        a[{11, 4}].write(0x00);
        ctx->run(0);
        REQUIRE(a.read() == 0xA00D);
    }

    SECTION("bits of the value above the slice width are ignored")
    {
        a[{7, 4}].write(0xFFF3);
        ctx->run(0);
        REQUIRE(a.read() == 0x0030);
    }

    SECTION("a slice write after a whole-signal write modifies the pending value")
    {
        a.write(0x1111);
        a[{3, 0}].write(0xF);
        ctx->run(0);
        REQUIRE(a.read() == 0x111F);
    }

    SECTION("signed signals stay sign extended from their width")
    {
        s[{23, 0}].write(0xFFFFF5);
        ctx->run(0);
        REQUIRE(s.read() == -11);

        // Clearing the top byte makes the value positive.
        s[{23, 16}].write(0);
        ctx->run(0);
        REQUIRE(s.read() == 0xFFF5);
    }
}

TEST_CASE("pack", "[bitsel]")
{
    auto ctx = Context::create();
    Signal<int16_t> tdata{"tdata", 16};
    Signal<uint8_t> tid{"tid"};
    Signal<uint8_t> tvalid{"tvalid", 1};
    tdata.init(-11);
    tid.init(3);
    tvalid.init(1);
    ctx->elaborate();

    // The first argument is the most significant, as in SystemVerilog {tdata, tid}.
    auto p = pack(tdata, tid);
    REQUIRE(p.width() == 24);
    REQUIRE(p.read() == 0xFFF503);
    REQUIRE(p.parts().size() == 2);
    REQUIRE(p.parts()[0] == BitPart{&tid, 7, 0});
    REQUIRE(p.parts()[1] == BitPart{&tdata, 15, 0});

    REQUIRE(sext(p[{23, 8}].read(), 16) == -11);
    REQUIRE(p[{7, 0}].read() == 3);

    auto p3 = pack(tvalid, tdata, tid);
    REQUIRE(p3.width() == 25);
    REQUIRE(p3.read() == 0x1FFF503);

    // Packs of packs are flattened.
    REQUIRE(pack(tvalid, p).parts() == p3.parts());

    p.write(0x123456);
    ctx->run(0);
    REQUIRE(tdata.read() == 0x1234);
    REQUIRE(tid.read() == 0x56);

    // Writing to a slice spanning two signals only changes the selected bits.
    p[{11, 4}].write(0x00);
    ctx->run(0);
    REQUIRE(tdata.read() == 0x1230);
    REQUIRE(tid.read() == 0x06);

    // Values written to signed signals through a pack are sign extended.
    p.write(0x800000);
    ctx->run(0);
    REQUIRE(tdata.read() == -32768);
}

TEST_CASE("pack and slice flattening", "[bitsel]")
{
    auto ctx = Context::create();
    Signal<uint8_t> a{"a"};
    Signal<uint8_t> b{"b"};
    a.init(0x12);
    b.init(0x34);

    auto p = pack(a, b);

    // A slice of a pack refers to the underlying signals.
    auto mid = p[{11, 4}];
    REQUIRE(mid.read() == 0x23);
    REQUIRE(mid.parts() == std::vector<BitPart>{{&b, 7, 4}, {&a, 3, 0}});

    // Adjacent ranges of the same signal are merged.
    auto whole = pack(a[{7, 4}], a[{3, 0}]);
    REQUIRE(whole.parts() == std::vector<BitPart>{{&a, 7, 0}});
    REQUIRE(pack(a, b)[{15, 8}].parts() == std::vector<BitPart>{{&a, 7, 0}});

    // Ranges in a different order are not.
    auto swapped = pack(a[{3, 0}], a[{7, 4}]);
    REQUIRE(swapped.parts().size() == 2);
    REQUIRE(swapped.read() == 0x21);

    // The same bits can be selected more than once.
    REQUIRE(pack(a, a).read() == 0x1212);
}

TEST_CASE("pack signal arrays", "[bitsel]")
{
    auto ctx = Context::create();
    SignalArray<uint8_t> arr{"arr", {2, 2}, 4};
    Signal<uint8_t> x{"x"};
    arr[{0, 0}].init(0x1);
    arr[{0, 1}].init(0x2);
    arr[{1, 0}].init(0x3);
    arr[{1, 1}].init(0x4);
    x.init(0xAB);
    ctx->elaborate();

    // Row-major with element 0 most significant, like {arr[0][0], arr[0][1], arr[1][0], arr[1][1]}.
    auto p = pack(arr);
    REQUIRE(p.width() == 16);
    REQUIRE(p.read() == 0x1234);

    REQUIRE(pack(arr.view()).read() == 0x1234);
    REQUIRE(pack(arr.slice({Slice::at(1)})).read() == 0x34);
    REQUIRE(pack(x, arr.slice({Slice::all(), Slice::at(0)}), x[{3, 0}]).read() == 0xAB13B);

    p.write(0xFEDC);
    ctx->run(0);
    REQUIRE(arr[{0, 0}].read() == 0xF);
    REQUIRE(arr[{0, 1}].read() == 0xE);
    REQUIRE(arr[{1, 0}].read() == 0xD);
    REQUIRE(arr[{1, 1}].read() == 0xC);
}

TEST_CASE("bit selection errors", "[bitsel]")
{
    auto ctx = Context::create();
    Signal<uint16_t> a{"a"};
    Signal<uint64_t> wide{"wide"};
    Signal<double> real{"real"};

    REQUIRE_THROWS_AS(a.slice(16, 0), std::out_of_range);
    REQUIRE_THROWS_AS(a.slice(3, 4), std::out_of_range);
    REQUIRE_THROWS_AS(a.slice(3, -1), std::out_of_range);
    REQUIRE_THROWS_AS(a[16], std::out_of_range);
    REQUIRE_THROWS_AS((a[{7, 0}][{8, 0}]), std::out_of_range);

    REQUIRE_THROWS_AS(real.slice(0, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(pack(a, real), std::invalid_argument);

    // Selections are limited to 64 bits.
    REQUIRE(pack(wide).width() == 64);
    REQUIRE_THROWS_AS(pack(wide, a[0]), std::invalid_argument);
    REQUIRE_THROWS_AS(BitSel::concat({}), std::invalid_argument);
}
