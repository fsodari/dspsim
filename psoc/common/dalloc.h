#pragma once
#include "dpool.h"
// #include <stdlib.h>

static inline void *dalloc(size_t amount)
{
    // return malloc(amount);
    return pool_alloc(amount);
}

static inline void dfree(void *x)
{
    // free(x);
    pool_free(x);
}