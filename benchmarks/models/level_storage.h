#pragma once
#include <cstddef>
#include <new>

namespace benchmarks
{
    /*
        Memory for the levels of a nested design (Chain, ChainSC): one block that holds them back to back,
        outermost first, the way members inside members are laid out.

        The levels used to be exactly that, a template that held the next level as a member. Compiling it
        instantiates the template once per level: MSVC gives up at a few hundred levels (error C1202), GCC
        needs minutes for 500, and the whole design was one object on the stack of the caller (more than the
        1 MB default stack on Windows for 500 levels). Allocating each level on its own is not an
        alternative, because it changes the results: the SystemC designs ran 13 to 18% slower that way.
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
}
