#pragma once
#include "dspsim/psoc/mmi.h"

struct MIterDef
{
    MMI mmi;
    uint32_t address;
};

static inline void inext(MIter it)
{
    it->mmi->next(it);
}

static inline int ieq(MIter it0, MIter it1)
{
    return it0->address == it1->address;
}

#define miter_pair_create(mmi, begin, end, it) \
    do                                         \
    {                                          \
        miter_pair(mmi, &begin, &end, &it);    \
    } while (0);

#define miter_pair_destroy(begin, end, it) \
    do                                     \
    {                                      \
        miter_destroy(&begin);             \
        miter_destroy(&end);               \
        miter_destroy(&it);                \
    } while (0);

// Pass an mmi, begin and end iterators, and name of iterator.
#define for_miter(mmi, begin, end, it)      \
    do                                      \
    {                                       \
        miter_pair(mmi, &begin, &end, &it); \
    } while (0);                            \
    for (; !ieq(it, end); inext(it))

// Clean up iterators.
#define endfor_miter(begin, end, it) miter_pair_destroy(begin, end, it)

static inline uint32_t miter_write(MIter it, const void *src)
{
    return mmi_write(it->mmi, it->address, src, dtype_size(it->mmi->dtype));
}

static inline uint32_t miter_read(MIter it, void *dst)
{
    return mmi_read(it->mmi, it->address, dst, mmi_dtype_size(it->mmi->dtype));
}

static inline uint32_t iget(MIter it, void *dst) { return miter_read(it, dst); }
static inline uint32_t iset(MIter it, const void *src) { return miter_write(it, src); }

// Typical standard iter functions.

// Increment the address.
static inline void miter_next_inc(MIter it) { it->address += dtype_size(it->mmi->dtype); }

void miter_init(MIter self, MMI mmi, uint32_t address);
MIter miter_create(MMI mmi, uint32_t address);
MIter miter_begin(MMI mmi);
MIter miter_end(MMI mmi);

void miter_pair(MMI mmi, MIter *begin, MIter *end, MIter *it);

void miter_destroy(MIter *self);

void icopy(MIter ibegin, MIter iend, MIter obegin);
