#include <dspsim/dspsim.h>
#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    class ArrTop : public Module
    {
    public:
        InputArray<int> i{"i", {2, 3}};
        OutputArray<int> o{"o", {2, 2, 2}};
        SignalArray<int> s{"s", {2, 3}};
        SignalArray<int> s3{"s3", {2, 2, 2}};

        ArrTop(ModuleName name) : Module(name)
        {
            DSPSIM_CORO(eval)->always(i);
            o.bind(s3);
        }

        Task eval()
        {
            while (true)
            {
                for (size_t a = 0; a < 2; ++a)
                    for (size_t b = 0; b < 3; ++b)
                        s[{a, b}] = i[{a, b}].read() * 2;
                o[{1, 0, 1}].write(i[{1, 2}].read());
                co_await wait();
            }
        }
    };

    class PortsTop : public Module
    {
    public:
        InputArray<int> in{"in", {2, 2}};
        OutputArray<int> out{"out", {2, 2}};
        PortsTop(ModuleName name) : Module(name) {}
    };

    class SliceTop : public Module
    {
    public:
        InputArray<int> in{"in", {2, 3}};
        OutputArray<int> out{"out", {2, 3}};
        SliceTop(ModuleName name) : Module(name) {}
    };
}

TEST_CASE("multidimensional signals and ports")
{
    auto ctx = Context::create();
    ArrTop top_inst{"top"};
    auto *top = &top_inst;
    SignalArray<int> in{"in", {2, 3}};
    top->i.bind(in);

    REQUIRE(top->s.size() == 6);
    REQUIRE(top->s3.extent(2) == 2);
    REQUIRE(top->s.extent(1) == 3);
    REQUIRE(&top->s[{1, 2}] == &top->s.flat(5));
    REQUIRE(top->s[{1, 2}].name() == "s[1][2]");
    REQUIRE_THROWS(top->s.at({2, 0}));
    REQUIRE_THROWS(top->s.at({1}));
    REQUIRE(top->s3.ndim() == 3);

    ctx->elaborate();
    in[{1, 2}] = 7;
    in[{0, 1}] = 3;
    ctx->run(1);
    REQUIRE(top->s[{1, 2}].read() == 14);
    REQUIRE(top->s[{0, 1}].read() == 6);
    REQUIRE(top->s3[{1, 0, 1}].read() == 7);
}

TEST_CASE("array iteration")
{
    auto ctx = Context::create();
    SignalArray<int> s{"s", {2, 3}};
    std::size_t n = 0;
    for (auto &e : s)
    {
        REQUIRE(&e == &s.flat(n));
        ++n;
    }
    REQUIRE(n == 6);
    REQUIRE(s.end() - s.begin() == 6);
    PortsTop top{"top"};
    REQUIRE(std::distance(top.in.begin(), top.in.end()) == 4);
    REQUIRE(std::distance(top.out.begin(), top.out.end()) == 4);
}

TEST_CASE("array slicing")
{
    auto ctx = Context::create();
    SignalArray<int> s{"s", {4, 6}};

    auto v = s.slice({Slice{1, 4}, Slice{0, std::nullopt, 2}});
    REQUIRE(v.shape() == Shape{3, 3});
    REQUIRE(&v[{0, 0}] == &s[{1, 0}]);
    REQUIRE(&v[{2, 2}] == &s[{3, 4}]);

    // Negative indices, reversed stride, clamping and dropped dimensions.
    auto r = s.slice({Slice::at(-1), Slice{std::nullopt, std::nullopt, -1}});
    REQUIRE(r.shape() == Shape{6});
    REQUIRE(&r.flat(0) == &s[{3, 5}]);
    REQUIRE(&r.flat(5) == &s[{3, 0}]);
    REQUIRE(s.slice({Slice{-2, 100}}).shape() == Shape{2, 6});
    REQUIRE(s.slice({Slice{3, 1}}).size() == 0);
    REQUIRE_THROWS(s.slice({Slice::at(4)}));
    REQUIRE_THROWS(s.slice({Slice{0, 1, 0}}));
    REQUIRE_THROWS(s.slice({Slice::all(), Slice::all(), Slice::all()}));

    // Slicing a slice.
    auto vv = v.slice({Slice::all(), Slice{1, std::nullopt}});
    REQUIRE(vv.shape() == Shape{3, 2});
    REQUIRE(&vv[{1, 0}] == &s[{2, 2}]);

    // Bind port slices to signal slices.
    SliceTop slice_top{"slice_top"};
    auto &in = slice_top.in;
    auto &out = slice_top.out;
    SignalArray<int> a{"a", {4, 6}};
    SignalArray<int> b{"b", {4, 6}};
    in.bind(a.slice({Slice{0, 4, 2}, Slice{0, 6, 2}}));
    out.slice({Slice{0, 1}}).bind(b.slice({Slice{3, 4}, Slice{0, 3}}));
    REQUIRE_THROWS(in.bind(a.slice({Slice{0, 2}})));

    ctx->elaborate();
    a[{2, 4}] = 9;
    ctx->run(1);
    REQUIRE(in[{1, 2}].read() == 9);
    REQUIRE(in[{0, 0}].read() == 0);
}

TEST_CASE("array bulk read and write")
{
    auto ctx = Context::create();
    SliceTop top{"top"};
    SignalArray<int> a{"a", {2, 3}};
    SignalArray<int> b{"b", {2, 3}};
    top.in.bind(a);
    top.out.bind(b);
    ctx->elaborate();

    a.write({1, 2, 3, 4, 5, 6});
    top.out.write({6, 5, 4, 3, 2, 1});
    a.slice({Slice::at(1)}).write(9);
    top.out.slice({Slice::all(), Slice{0, 1}}).write({0, 7});
    REQUIRE_THROWS(a.write({1, 2}));
    ctx->run(1);

    REQUIRE(top.in.read() == std::vector<int>{1, 2, 3, 9, 9, 9});
    REQUIRE(top.in.slice({Slice::at(1)}).read() == std::vector<int>{9, 9, 9});
    REQUIRE(b.read() == std::vector<int>{0, 5, 4, 7, 2, 1});
    REQUIRE(b.slice({Slice::all(), Slice{0, 3, 2}}).read() == std::vector<int>{0, 4, 7, 1});
}
