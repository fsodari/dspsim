#include "FreeRTOS.h"
#include "portable.h"
#include "dspsim/psoc/cdict.h"
#include "dspsim/psoc/error_codes.h"
#include <string.h>

struct DictBinDef
{
    // Un-hashed key. Pointer to allocated memory.
    const void *key;
    // Stored value. Pointer to allocated memory.
    void *value;
    // Size of value in bytes. Dict entries can have arbitrary size.
    uint32_t value_size;

    // Next bin in the list. Null if this bin is the tail.
    DictBin next;
};

struct DictDef
{
    // Number of bins. Bin index is hash % n_bins
    uint32_t n_bins;

    // Hashing function.
    Hash hash;

    // Number of elements in the dict.
    uint32_t size;

    /*
        Array of bins. Fixed number of bins. (Reallocate?)
        Each entry is a list of bins to handle collisions.
        bin_index = hash % n_bins
        On collision, search the list in the bin_index for the matched key.
    */
    DictBin *bins;
};

DictBin _create_bin(Dict dict, const void *key, const void *value, uint32_t value_size)
{
    DictBin self = pvPortMalloc(sizeof(*self));

    // Allocate and copy key into bin.
    uint32_t dsize = dtype_size(dict->hash->dtype);
    self->key = pvPortMalloc(dsize);
    if (dtype_is_str(dict->hash->dtype))
    {
        strncpy((char *)self->key, (char *)key, dsize);
    }
    else
    {
        memcpy((void *)self->key, key, dsize);
    }

    // Allocate and copy value into bin.
    self->value = pvPortMalloc(value_size);
    memcpy(self->value, value, value_size);
    self->value_size = value_size;

    // Next points to null indicating it is the last in the list.
    self->next = NULL;

    return self;
}

void _destroy_bin(DictBin bin)
{
    vPortFree(bin->value);
    vPortFree((void *)bin->key);
    vPortFree(bin);
}

void _destroy_bin_list(DictBin head)
{
    if (head != NULL)
    {
        _destroy_bin_list(head->next);
        // Destroy self.
        _destroy_bin(head);
    }
}

void _append_bin(DictBin *next, DictBin bin)
{
    // pass next recursively. if the *hea
    if (*next)
    {
        _append_bin(&(*next)->next, bin);
    }
    else
    {
        *next = bin;
    }
}

// DictBin _find_hash(Dict dict, uint32_t hash, )

DictBin _search_bin_list(Dict dict, DictBin bin, const void *key)
{
    DictBin found = NULL;
    if (bin != NULL)
    {
        // Search the tail
        found = _search_bin_list(dict, bin->next, key);
        if (!found)
        {
            // Didn't find it in the tail, check this bin
            found = hash_compare(dict->hash, bin->key, key) ? bin : NULL;
        }
    }
    return found;
}

Dict dict_create(uint32_t n_bins, Hash hash)
{
    Dict self = pvPortMalloc(sizeof(*self));

    self->n_bins = n_bins;
    self->hash = hash;
    self->size = 0;

    // Allocate bin array. Pointers to bins. The bins are not allocated yet.
    self->bins = pvPortMalloc(n_bins * sizeof(*self->bins));
    for (uint32_t i = 0; i < n_bins; i++)
    {
        self->bins[i] = NULL;
    }

    return self;
}

void dict_destroy(Dict self)
{
    // Delete all bins.
    for (uint32_t i = 0; i < self->n_bins; i++)
    {
        _destroy_bin_list(self->bins[i]);
    }
}

uint32_t _dict_bin_id(const Dict self, const void *key)
{
    return hash_hash(self->hash, key) % self->n_bins;
}

// Find the bin with the given key.
DictBin dict_find(const Dict self, const void *key)
{
    uint32_t bin_id = _dict_bin_id(self, key);
    DictBin found = _search_bin_list(self, self->bins[bin_id], key);

    return found;
}

// Return pointer to value. If the key doesn't exist, returns NULL
void *dict_ref(Dict self, const void *key)
{
    DictBin found = dict_find(self, key);
    if (found)
    {
        return found->value;
    }
    else
    {
        return NULL;
    }
}
uint32_t dict_size(Dict self)
{
    return self->size;
}

enum dErrorCodes _dict_create_append(Dict self, const void *key, const void *value, uint32_t size)
{
    dErrorCodes error = dErrNone;

    DictBin bin = _create_bin(self, key, value, size);
    uint32_t bin_id = _dict_bin_id(self, key);

    _append_bin(&self->bins[bin_id], bin);
    self->size++;

    return error;
}

dErrorCodes dict_set(Dict self, const void *key, const void *value, uint32_t size)
{
    dErrorCodes error = dErrNone;

    DictBin found = dict_find(self, key);
    // if not found, create a new bin and append it to the appropriate bin.
    if (!found)
    {
        error = _dict_create_append(self, key, value, size);
    }
    else
    {
        // Overwrite the value.
        memcpy(found->value, value, size);
    }

    return error;
}

uint32_t dict_value_size(const Dict self, const void *key)
{
    DictBin found = dict_find(self, key);

    if (found)
    {
        return found->value_size;
    }
    else
    {
        return 0;
    }
}

uint32_t dict_get(Dict self, const void *key, void *value)
{
    uint32_t copied = 0;

    DictBin found = dict_find(self, key);

    if (found)
    {
        memcpy(value, found->value, found->value_size);
        copied = found->value_size;
    }

    return copied;
}

// Safely read a value by supplying max read size; Returns error code if key not found.
dErrorCodes dict_read(Dict self, const void *key, void *value, uint32_t max_size)
{
    dErrorCodes error = dErrNone;
    DictBin found = dict_find(self, key);

    if (!found)
    {
        return dErrKeyNotFound;
    }

    if (found->value_size <= max_size)
    {
        memcpy(value, found->value, found->value_size);
    }
    else
    {
        error = dErrOverflow;
    }

    return error;
}
