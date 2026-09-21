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
        Input<T> a;
        Input<T> b;
        Output<T> c;

        Adder(ModuleName name, Signal<T> &a_, Signal<T> &b_, Signal<T> &c_)
            : Module(name),
              a("a", a_),
              b("b", b_),
              c("c", c_)
        {
            always << a << b;
        }

        void eval() override
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
        Input<T> a1;
        Input<T> a2;
        Input<T> b1;
        Input<T> b2;
        Input<T> c1;
        Input<T> c2;
        Input<T> d1;
        Input<T> d2;
        Output<T> go;

        // Submodules
        Adder<T> adder_a;
        Adder<T> adder_b;
        Adder<T> adder_c;
        Adder<T> adder_d;
        Adder<T> adder_e;
        Adder<T> adder_f;
        Adder<T> adder_g;

        AdderTree(ModuleName name,
                  Signal<T> &a1_, Signal<T> &a2_,
                  Signal<T> &b1_, Signal<T> &b2_,
                  Signal<T> &c1_, Signal<T> &c2_,
                  Signal<T> &d1_, Signal<T> &d2_,
                  Signal<T> &go_)
            : Module(name),
              a1("a1", a1_),
              a2("a2", a2_),
              b1("b1", b1_),
              b2("b2", b2_),
              c1("c1", c1_),
              c2("c2", c2_),
              d1("d1", d1_),
              d2("d2", d2_),
              go("go", go_),
              adder_a("adder_a", a1, a2, ao),
              adder_b("adder_b", b1, b2, bo),
              adder_c("adder_c", c1, c2, co),
              adder_d("adder_d", d1, d2, do_),
              adder_e("adder_e", ao, bo, eo),
              adder_f("adder_f", co, do_, fo),
              adder_g("adder_g", eo, fo, go)
        {
            // always << a1 << a2 << b1 << b2 << c1 << c2 << d1 << d2;
        }

        void eval() override
        {
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
        Input<T> a1;
        Input<T> a2;
        Input<T> b1;
        Input<T> b2;
        Output<T> co;

        // Submodules
        Adder<T> adder_a;
        Adder<T> adder_b;
        Adder<T> adder_c;

        Adder2(ModuleName name,
               Signal<T> &a1_, Signal<T> &a2_,
               Signal<T> &b1_, Signal<T> &b2_, Signal<T> &co_)
            : Module(name),
              a1("a1", a1_),
              a2("a2", a2_),
              b1("b1", b1_),
              b2("b2", b2_),
              co("co", co_),
              adder_a("adder_a", a1, a2, ao),
              adder_b("adder_b", b1, b2, bo),
              adder_c("adder_c", ao, bo, co)
        {
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
        Input<T> a1;
        Input<T> a2;
        Input<T> b1;
        Input<T> b2;
        Input<T> c1;
        Input<T> c2;
        Input<T> d1;
        Input<T> d2;
        Output<T> eo;

        // Submodules
        Adder2<T> adder_ab;
        Adder2<T> adder_cd;
        Adder<T> adder_e;

        AdderNested(ModuleName name,
                    Signal<T> &a1_, Signal<T> &a2_,
                    Signal<T> &b1_, Signal<T> &b2_,
                    Signal<T> &c1_, Signal<T> &c2_,
                    Signal<T> &d1_, Signal<T> &d2_,
                    Signal<T> &eo_)
            : Module(name),
              a1("a1", a1_),
              a2("a2", a2_),
              b1("b1", b1_),
              b2("b2", b2_),
              c1("c1", c1_),
              c2("c2", c2_),
              d1("d1", d1_),
              d2("d2", d2_),
              eo("eo", eo_),
              adder_ab("adder_ab", a1, a2, b1, b2, abo),
              adder_cd("adder_cd", c1, c2, d1, d2, cdo),
              adder_e{"adder_e", abo, cdo, eo}
        {
        }
    };
}

TEST_CASE("test_adder_tree")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::trace);

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

    Adder<int> adder_a1{"adder_a1", ai1, ai2, ao};
    Adder<int> adder_b1{"adder_b1", bi1, bi2, bo};
    Adder<int> adder_c1{"adder_c1", ci1, ci2, co};
    Adder<int> adder_d1{"adder_d1", di1, di2, do_};

    Adder<int> adder_e2{"adder_e2", ao, bo, eo};
    Adder<int> adder_f2{"adder_f2", co, do_, fo};

    Adder<int> adder_g3{"adder_g3", eo, fo, go};

    // Compare to adder tree in a module.
    AdderTree<int> adder_tree{"adder_tree", ai1, ai2, bi1, bi2, ci1, ci2, di1, di2, adder_tree_o};

    // Compare to adder tree with nested submodule.
    AdderNested<int> adder_nested{"adder_nested", ai1, ai2, bi1, bi2, ci1, ci2, di1, di2, adder_nested_o};

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