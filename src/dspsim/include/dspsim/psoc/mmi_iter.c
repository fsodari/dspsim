#include "FreeRTOS.h"
#include "dspsim/psoc/mmi_iter.h"

static uint8_t _swap_buf[256];
uint8_t *mmi_swap_buf = _swap_buf;

void miter_init(MIter self, MMI mmi, uint32_t address)
{
    self->mmi = mmi;
    self->address = address;
}
MIter miter_create(MMI mmi, uint32_t address)
{
    MIter self = pvPortMalloc(sizeof(*self));
    miter_init(self, mmi, address);
    return self;
}

MIter miter_begin(MMI mmi)
{
    return miter_create(mmi, 0);
}

MIter miter_end(MMI mmi)
{
    return miter_create(mmi, mmi->size);
}

void miter_pair(MMI mmi, MIter *begin, MIter *end, MIter *it)
{
    *begin = miter_begin(mmi);
    *end = miter_end(mmi);
    *it = miter_begin(mmi);
}

void miter_destroy(MIter *self)
{
    vPortFree(*self);
    *self = NULL;
}
void icopy(MIter ibegin, MIter iend, MIter obegin)
{
    struct MIterDef _iit = *ibegin;
    struct MIterDef _oit = *obegin;
    for (MIter iit = &_iit, oit = &_oit; !ieq(iit, iend); inext(iit), inext(oit))
    {
        iget(iit, mmi_swap_buf);
        iset(oit, mmi_swap_buf);
    }
}