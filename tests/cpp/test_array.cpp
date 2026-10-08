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
    InputArray<int> in{"in", {2, 2}};
    OutputArray<int> out{"out", {2, 2}};
    REQUIRE(std::distance(in.begin(), in.end()) == 4);
    REQUIRE(std::distance(out.begin(), out.end()) == 4);
}
