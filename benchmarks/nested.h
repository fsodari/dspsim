#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include "eval_funcs.h"

#include <cstddef>
#include <new>

namespace benchmarks
{
    /*
        Nested designs: every level holds a model followed by the next level, nested inside it.

            in -> some_model -> sig -> nested -> out
            in -> some_model -> out                     (innermost level, which leaves sig unused)

        The number of levels is a constructor argument. It used to be a template parameter, with level N
        holding level N - 1 as a member. Compiling that instantiates the template N levels deep: MSVC gives up
        at a few hundred levels (error C1202), GCC needs minutes for 500, and the whole design was one object
        on the stack of the caller (more than the 1 MB default stack on Windows for 500 levels).

        The levels are still laid out the way members inside members are: back to back in one block of
        memory, outermost first (LevelStorage). Allocating each level on its own changes the results: the
        SystemC designs ran 13 to 18% slower that way.
    */
    template <typename Level>
    class LevelStorage
    {
    public:
        explicit LevelStorage(int levels)
            : block_(::operator new(sizeof(Level) * static_cast<std::size_t>(levels > 0 ? levels : 1),
                                    std::align_val_t(alignof(Level))))
        {
        }
        LevelStorage(const LevelStorage &) = delete;
        LevelStorage &operator=(const LevelStorage &) = delete;
        ~LevelStorage()
        {
            ::operator delete(block_, std::align_val_t(alignof(Level)));
        }

        /// Memory for the next level. The caller constructs the level in it, and destroys it.
        void *next()
        {
            return static_cast<std::byte *>(block_) + sizeof(Level) * used_++;
        }

    private:
        void *block_;
        std::size_t used_ = 0;
    };

    /// One level of a dspsim design. The outermost level is a NestedComb.
    template <typename M, typename T>
    DSPSIM_MODULE(NestedLevel)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Signal<T> sig{"sig"};

        M some_model{"some_model"};
        NestedLevel *nested = nullptr;

        NestedLevel(::dspsim::ModuleName, int depth, LevelStorage<NestedLevel> &storage)
        {
            some_model.clk.bind(clk);
            some_model.in.bind(in);
            if (depth > 0)
            {
                nested = new (storage.next()) NestedLevel("nested", depth - 1, storage);
                some_model.out.bind(sig);

                nested->clk.bind(clk);
                nested->in.bind(sig);
                nested->out.bind(out);
            }
            else
            {
                some_model.out.bind(out);
            }
        }
        NestedLevel(const NestedLevel &) = delete;
        NestedLevel &operator=(const NestedLevel &) = delete;
        ~NestedLevel()
        {
            if (nested)
            {
                nested->~NestedLevel();
            }
        }
    };

    /// A dspsim design of DEPTH + 1 nested levels.
    template <typename M, typename T, int DEPTH>
    struct NestedComb : private LevelStorage<NestedLevel<M, T>>, public NestedLevel<M, T>
    {
        NestedComb(::dspsim::ModuleName name)
            : LevelStorage<NestedLevel<M, T>>(DEPTH), NestedLevel<M, T>(name, DEPTH, *this) {}
    };

    /// One level of a SystemC design. The outermost level is a NestedCombSC.
    template <typename M, typename T>
    class NestedLevelSC : public sc_core::sc_module
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_signal<T> sig{"sig"};

        M some_model{"some_model"};
        NestedLevelSC *nested = nullptr;

        NestedLevelSC(sc_core::sc_module_name name, int depth, LevelStorage<NestedLevelSC> &storage)
            : sc_core::sc_module(name)
        {
            some_model.clk.bind(clk);
            some_model.in.bind(in);
            if (depth > 0)
            {
                nested = new (storage.next()) NestedLevelSC("nested", depth - 1, storage);
                some_model.out.bind(sig);

                nested->clk.bind(clk);
                nested->in.bind(sig);
                nested->out.bind(out);
            }
            else
            {
                some_model.out.bind(out);
            }
        }
        ~NestedLevelSC() override
        {
            if (nested)
            {
                nested->~NestedLevelSC();
            }
        }
    };

    /// A SystemC design of DEPTH + 1 nested levels.
    template <typename M, typename T, int DEPTH>
    class NestedCombSC : private LevelStorage<NestedLevelSC<M, T>>, public NestedLevelSC<M, T>
    {
    public:
        NestedCombSC(sc_core::sc_module_name name)
            : LevelStorage<NestedLevelSC<M, T>>(DEPTH), NestedLevelSC<M, T>(name, DEPTH, *this) {}
    };
}
