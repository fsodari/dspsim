#include "dpool.h"

static uint8_t *_mem_begin = NULL;
static uint8_t *_mem_end = NULL;
static uint8_t *_next_block = NULL;

void pool_init(uint8_t *mem, size_t size)
{
    _mem_begin = mem;
    _mem_end = mem + size;

    _next_block = _mem_begin;
}

void pool_zeros()
{
    for (uint8_t *m = _mem_begin; m != _mem_end; m++)
    {
        *m = 0;
    }
}

void *pool_alloc(size_t size)
{
    int remainder = size % POOL_MIN_BLOCK_SIZE;
    size += (POOL_MIN_BLOCK_SIZE - remainder);

    void *result = _next_block;
    _next_block += size;

    return result;
}

void pool_free(void *x)
{
}
