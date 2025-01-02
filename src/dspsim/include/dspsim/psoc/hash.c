#include "FreeRTOS.h"
#include "dspsim/psoc/hash.h"
#include <string.h>
#include <stdlib.h>

void hash_init(Hash self, hash_hash_ft hash, hash_compare_ft compare, DType dtype)
{
    self->hash = hash;
    self->compare = compare;
    self->dtype = dtype;
}

Hash hash_create(hash_hash_ft hash, hash_compare_ft compare, DType dtype)
{
    Hash self = pvPortMalloc(sizeof(*self));
    hash_init(self, hash, compare, dtype);
    return self;
}

// Standard hash functions
#define HASH_INIT 2166136261
#define HASH_P 16777619

// Standard hash functions and hash singletons.
// Bytes hash
static inline void _byte_hash_step(uint32_t *hash, uint8_t b)
{
    static const uint32_t p = HASH_P;
    *hash = (*hash ^ b) * p;
}

uint32_t hash_bytes_func(void *ref, const void *_src, uint32_t size)
{
    (void)ref;
    const uint8_t *src = _src;

    uint32_t hash = HASH_INIT;

    for (uint32_t i = 0; i < size; i++)
    {
        _byte_hash_step(&hash, src[i]);
    }

    return hash;
}
int hash_compare_bytes_func(void *ref, const void *x1, const void *x2, uint32_t size)
{
    return memcmp(x1, x2, size) == 0;
}

// uint32_t hash
uint32_t hash_32_func(void *ref, const void *_src, uint32_t size)
{
    (void)size; // Ignore size, always 4.
    return hash_bytes_func(ref, _src, sizeof(uint32_t));
}

int hash_compare_32_func(void *ref, const void *_x1, const void *_x2, uint32_t size)
{
    (void)ref;
    const uint32_t *x1 = _x1, *x2 = _x2;
    return *x1 == *x2;
}

static struct HashDef _hash_32 = {hash_32_func, hash_compare_32_func, sizeof(uint32_t)};
Hash const hash_32 = &_hash_32;

// string/char* hash
uint32_t hash_str_func(void *ref, const void *_src, uint32_t max_size)
{
    const char *src = _src;
    uint32_t hash = HASH_INIT;

    for (uint32_t i = 0; i < max_size; i++)
    {
        char b = src[i];
        _byte_hash_step(&hash, b);
        if (b == '\0')
        {
            break;
        }
    }
    return hash;
}
int hash_compare_str_func(void *ref, const void *_x1, const void *_x2, uint32_t max_size)
{
    const char *x1 = _x1, *x2 = _x2;
    return strncmp(x1, x2, max_size) == 0;
}
