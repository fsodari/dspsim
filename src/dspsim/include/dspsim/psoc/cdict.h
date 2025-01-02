#pragma once
#include "dspsim/psoc/hash.h"
#include "dspsim/psoc/error_codes.h"
#include "dspsim/psoc/dtypes.h"
#include <stdint.h>

typedef uint32_t (*dict_hash_ft)(void *ref, const void *key, uint32_t key_size);
typedef int (*dict_compare_ft)(void *ref, const void *key1, const void *key2, uint32_t key_size);

typedef struct DictBinDef *DictBin;
typedef struct DictDef *Dict;

/*
    Create a new Dict with n_bins. Entrys are dynamically created so this can hold an arbitrary number of elements.
    Any type of key can be used if a given
*/
Dict dict_create(uint32_t n_bins, Hash hash);
// Dict dict_create(uint32_t n_bins, MMIDtypes key_type);

void dict_destroy(Dict self);

void dict_set_hash_func(Dict self, uint32_t key_size, dict_hash_ft hash_func, dict_compare_ft compare_func, void *ref);

dErrorCodes dict_set(Dict self, const void *key, const void *value, uint32_t size);

// Get the value size of the key.
uint32_t dict_value_size(const Dict self, const void *key);

// Find the bin with the given key.
DictBin dict_find(const Dict self, const void *key);

// Get a value from the dict. Returns size copied, or 0 if the key was not found.
uint32_t dict_get(Dict self, const void *key, void *value);

// Safely read a value by supplying max read size; Returns error code if key not found.
enum dErrorCodes dict_read(Dict self, const void *key, void *value, uint32_t max_size);

// Return pointer to value. If the key doesn't exist, returns NULL
void *dict_ref(Dict self, const void *key);

uint32_t dict_size(Dict self);

// Standard hash functions
// Hash bytes
uint32_t dict_hash_bytes(void *ref, const void *key, uint32_t key_size);
int dict_compare_bytes(void *ref, const void *key1, const void *key2, uint32_t key_size);

// uint32_t key.
#define dict_createl(n_bins) dict_create(n_bins, hash_32)
static inline dErrorCodes dict_setl(Dict self, uint32_t key, const void *value, uint32_t size)
{
    return dict_set(self, &key, value, size);
}
static inline uint32_t dict_getl(Dict self, uint32_t key, void *value)
{
    return dict_get(self, &key, value);
}

// char * key
#define dict_create_str(n_bins, dtype) dict_create(n_bins, hash_create_str(dtype))
static inline dErrorCodes dict_set_str(Dict self, const char *key, const void *value, uint32_t size)
{
    return dict_set(self, key, value, size);
}

#define dict_ref_str(dict, key) (const char *)dict_ref(dict, key)