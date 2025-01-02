#pragma once
#include "dspsim/psoc/dtypes.h"
#include <stdint.h>

// Required hashing functions
typedef uint32_t (*hash_hash_ft)(void *ref, const void *src, uint32_t size);
typedef int (*hash_compare_ft)(void *ref, const void *x1, const void *x2, uint32_t size);

typedef struct HashDef *Hash;
struct HashDef
{
    hash_hash_ft hash;
    hash_compare_ft compare;
    DType dtype;
};

void hash_init(Hash self, hash_hash_ft hash, hash_compare_ft compare, DType dtype);
Hash hash_create(hash_hash_ft hash, hash_compare_ft compare, DType dtype);

static inline uint32_t hash_hash(Hash self, const void *src)
{
    return self->hash(self, src, dtype_size(self->dtype));
}

static inline int hash_compare(Hash self, const void *x1, const void *x2)
{
    return self->compare(self, x1, x2, dtype_size(self->dtype));
}

// Standard hash functions and hash singletons.
// Bytes hash
uint32_t hash_bytes_func(void *ref, const void *src, uint32_t size);
int hash_compare_bytes_func(void *ref, const void *x1, const void *x2, uint32_t size);
#define hash_create_bytes(size) hash_create(hash_bytes_func, hash_compare_bytes_func, size);

// uint32_t hash
uint32_t hash_32_func(void *ref, const void *src, uint32_t size);
int hash_compare_32_func(void *ref, const void *x1, const void *x2, uint32_t size);
#define hash_create_32() hash_create(hash_32_func, hash_compare_32_func, sizeof(uint32_t))

// string/char* hash
uint32_t hash_str_func(void *ref, const void *src, uint32_t size);
int hash_compare_str_func(void *ref, const void *x1, const void *x2, uint32_t size);
#define hash_create_str(dtype) hash_create(hash_str_func, hash_compare_str_func, dtype)

// Singletons
extern Hash const hash_32;
