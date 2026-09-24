#include <dspsim/dspsim.h>

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    template <typename T>
    class Adder : public Module
    {
    public:
        Input<T> a{"a"};
        Input<T> b{"b"};
        Output<T> c{"c"};

        Adder(ModuleName name)
            : Module(name)
        {
            DSPSIM_METHOD(&Adder<T>::eval_)->always("*");
        }

        void eval_()
        {
            c.write(a.read() + b.read());
        }
    };

    // Module containing all adders in tree.
    template <typename T>
    class AdderTree : public Module
    {
    public:
        // Internal signals
        Signal<T> ao{"ao"};
        Signal<T> bo{"bo"};
        Signal<T> co{"co"};
        Signal<T> do_{"do"};
        Signal<T> eo{"eo"};
        Signal<T> fo{"fo"};

        // Ports
        Input<T> a1{"a1"};
        Input<T> a2{"a2"};
        Input<T> b1{"b1"};
        Input<T> b2{"b2"};
        Input<T> c1{"c1"};
        Input<T> c2{"c2"};
        Input<T> d1{"d1"};
        Input<T> d2{"d2"};
        Output<T> go{"go"};

        // Submodules
        Adder<T> adder_a{"adder_a"};
        Adder<T> adder_b{"adder_b"};
        Adder<T> adder_c{"adder_c"};
        Adder<T> adder_d{"adder_d"};
        Adder<T> adder_e{"adder_e"};
        Adder<T> adder_f{"adder_f"};
        Adder<T> adder_g{"adder_g"};

        AdderTree(ModuleName name)
            : Module(name)
        {
            adder_a.a.bind(a1);
            adder_a.b.bind(a2);
            adder_a.c.bind(ao);

            adder_b.a.bind(b1);
            adder_b.b.bind(b2);
            adder_b.c.bind(bo);

            adder_c.a.bind(c1);
            adder_c.b.bind(c2);
            adder_c.c.bind(co);

            adder_d.a.bind(d1);
            adder_d.b.bind(d2);
            adder_d.c.bind(do_);

            adder_e.a.bind(ao);
            adder_e.b.bind(bo);
            adder_e.c.bind(eo);

            adder_f.a.bind(co);
            adder_f.b.bind(do_);
            adder_f.c.bind(fo);

            adder_g.a.bind(eo);
            adder_g.b.bind(fo);
            adder_g.c.bind(go);
        }
    };

    // Module containing two adders.
    template <typename T>
    class Adder2 : public Module
    {
    public:
        // Internal signals
        Signal<T> ao{"ao"};
        Signal<T> bo{"bo"};

        // Ports
        Input<T> a1{"a1"};
        Input<T> a2{"a2"};
        Input<T> b1{"b1"};
        Input<T> b2{"b2"};
        Output<T> co{"co"};

        // Submodules
        Adder<T> adder_a{"adder_a"};
        Adder<T> adder_b{"adder_b"};
        Adder<T> adder_c{"adder_c"};

        Adder2(ModuleName name)
            : Module(name)
        {
            adder_a.a.bind(a1);
            adder_a.b.bind(a2);
            adder_a.c.bind(ao);

            adder_b.a.bind(b1);
            adder_b.b.bind(b2);
            adder_b.c.bind(bo);

            adder_c.a.bind(ao);
            adder_c.b.bind(bo);
            adder_c.c.bind(co);
        }
    };

    template <typename T>
    class AdderNested : public Module
    {
    public:
        /*
            Currently, in order for this to work, internal signals must be declared first,
            then ports, and finally submodules.

            This is a pain. It would be nicer if the order of declaration didn't matter.
            If true binding could be delayed until elaboration, then the order of declaration wouldn't matter.
        */
        // Internal signals
        Signal<T> abo{"abo"};
        Signal<T> cdo{"cdo"};

        // Ports
        Input<T> a1{"a1"};
        Input<T> a2{"a2"};
        Input<T> b1{"b1"};
        Input<T> b2{"b2"};
        Input<T> c1{"c1"};
        Input<T> c2{"c2"};
        Input<T> d1{"d1"};
        Input<T> d2{"d2"};
        Output<T> eo{"eo"};

        // Submodules
        Adder2<T> adder_ab{"adder_ab"};
        Adder2<T> adder_cd{"adder_cd"};
        Adder<T> adder_e{"adder_e"};

        AdderNested(ModuleName name)
            : Module(name)
        {
            adder_ab.a1.bind(a1);
            adder_ab.a2.bind(a2);
            adder_ab.b1.bind(b1);
            adder_ab.b2.bind(b2);
            adder_ab.co.bind(abo);

            adder_cd.a1.bind(c1);
            adder_cd.a2.bind(c2);
            adder_cd.b1.bind(d1);
            adder_cd.b2.bind(d2);
            adder_cd.co.bind(cdo);

            adder_e.a.bind(abo);
            adder_e.b.bind(cdo);
            adder_e.c.bind(eo);
        }
    };
}

TEST_CASE("test_adder_tree", "[adder_tree]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);

    // Clock clk{"clk", 10};
    Signal<int> ai1{"ai1"};
    Signal<int> ai2{"ai2"};
    Signal<int> ao{"ao"};

    Signal<int> bi1{"bi1"};
    Signal<int> bi2{"bi2"};
    Signal<int> bo{"bo"};

    Signal<int> ci1{"ci1"};
    Signal<int> ci2{"ci2"};
    Signal<int> co{"co"};

    Signal<int> di1{"di1"};
    Signal<int> di2{"di2"};
    Signal<int> do_{"do"};

    Signal<int> eo{"eo"};
    Signal<int> fo{"fo"};

    Signal<int> go{"go"};
    Signal<int> adder_tree_o{"adder_tree_o"};
    Signal<int> adder_nested_o{"adder_nested_o"};

    Adder<int> adder_a1{"adder_a1"};
    adder_a1.a.bind(ai1);
    adder_a1.b.bind(ai2);
    adder_a1.c.bind(ao);
    Adder<int> adder_b1{"adder_b1"};
    adder_b1.a.bind(bi1);
    adder_b1.b.bind(bi2);
    adder_b1.c.bind(bo);
    Adder<int> adder_c1{"adder_c1"};
    adder_c1.a.bind(ci1);
    adder_c1.b.bind(ci2);
    adder_c1.c.bind(co);
    Adder<int> adder_d1{"adder_d1"};
    adder_d1.a.bind(di1);
    adder_d1.b.bind(di2);
    adder_d1.c.bind(do_);

    Adder<int> adder_e2{"adder_e2"};
    adder_e2.a.bind(ao);
    adder_e2.b.bind(bo);
    adder_e2.c.bind(eo);
    Adder<int> adder_f2{"adder_f2"};
    adder_f2.a.bind(co);
    adder_f2.b.bind(do_);
    adder_f2.c.bind(fo);

    Adder<int> adder_g3{"adder_g3"};
    adder_g3.a.bind(eo);
    adder_g3.b.bind(fo);
    adder_g3.c.bind(go);

    // Compare to adder tree in a module.
    AdderTree<int> adder_tree{"adder_tree"};
    adder_tree.a1.bind(ai1);
    adder_tree.a2.bind(ai2);
    adder_tree.b1.bind(bi1);
    adder_tree.b2.bind(bi2);
    adder_tree.c1.bind(ci1);
    adder_tree.c2.bind(ci2);
    adder_tree.d1.bind(di1);
    adder_tree.d2.bind(di2);
    adder_tree.go.bind(adder_tree_o);

    // Compare to adder tree with nested submodule.
    AdderNested<int> adder_nested{"adder_nested"};
    adder_nested.a1.bind(ai1);
    adder_nested.a2.bind(ai2);
    adder_nested.b1.bind(bi1);
    adder_nested.b2.bind(bi2);
    adder_nested.c1.bind(ci1);
    adder_nested.c2.bind(ci2);
    adder_nested.d1.bind(di1);
    adder_nested.d2.bind(di2);
    adder_nested.eo.bind(adder_nested_o);

    ctx->elaborate();

    // Write to the adder inputs.
    ai1.write(1);
    ai2.write(2);
    bi1.write(3);
    bi2.write(4);
    ci1.write(5);
    ci2.write(6);
    di1.write(7);
    di2.write(8);

    // Everything should update in a single delta cycle
    ctx->run();
    REQUIRE(ao.read() == 3);
    REQUIRE(bo.read() == 7);
    REQUIRE(co.read() == 11);
    REQUIRE(do_.read() == 15);

    REQUIRE(eo.read() == 10);
    REQUIRE(fo.read() == 26);

    REQUIRE(go.read() == 36);

    REQUIRE(adder_tree_o.read() == 36);
    REQUIRE(adder_nested_o.read() == 36);
}