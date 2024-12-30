#pragma once
#include "dspsim/mmi.h"

typedef struct MIter MIter;
typedef void (*miter_next_ft)(MIter *);

struct MIter
{
    MMI *mmi_ref;
    uint32_t address;
    uint32_t inc_size;
    miter_next_ft next;
};

static inline void inext(MIter *it)
{
    it->next(it);
}
static inline int ieq(MIter *it0, MIter *it1)
{
    return (it0->address == it1->address);
}

/*
    for (MIter it = x_begin(&x), MIter end = x_end(&x); !ieq(&it, &end); inext(&it))
    {
        uint32_t x;
        iget(&it, &x);
        uint32_t y = 42;
        iset(&it, &y);
    }

*/

static inline uint32_t miter_write(MIter *it, const void *src)
{
    return mmi_write(it->mmi_ref, it->address, src, it->inc_size);
}

static inline uint32_t miter_read(MIter *it, void *dst)
{
    return mmi_read(it->mmi_ref, it->address, dst, it->inc_size);
}

static inline uint32_t iget(MIter *it, void *dst) { return miter_read(it, dst); }
static inline uint32_t iset(MIter *it, const void *src) { return miter_write(it, src); }

// Typical standard iter functions.
// Increment the address.
void miter_next_inc(MIter *it);
// Increment the address, and rollover
void miter_next_circ(MIter *it);

void icopy(MIter *ibegin, MIter *iend, MIter *obegin);
