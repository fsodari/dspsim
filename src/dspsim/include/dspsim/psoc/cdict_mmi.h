/*
    Memory mapped registers with a dict interface.
    Each register's address is a key.
*/
#pragma once
#include "dspsim/psoc/cdict.h"
#include "dspsim/psoc/mmi.h"
#include "dspsim/psoc/mmi_iter.h"
#include "dspsim/psoc/dtypes.h"

typedef struct DictMMIDef *DictMMI;
struct DictMMIDef
{
    struct MMIDef base;
    Dict dict;
};

DictMMI dict_mmi_create(uint32_t n_bins, uint32_t n_regs, DType dtype);
