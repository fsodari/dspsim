/*

    Memory mapped interface

*/
#pragma once
#include <stdint.h>

// Interfaces must implement these function types.
typedef uint32_t (*mmi_write_ft)(void *self, uint32_t address, const void *src, uint32_t size);
typedef uint32_t (*mmi_read_ft)(void *self, uint32_t address, void *dst, uint32_t size);

typedef struct MMIDef *MMI;
struct MMIDef
{
    mmi_write_ft write;
    mmi_read_ft read;
    uint32_t size;
};

void mmi_init(MMI self, mmi_write_ft write, mmi_read_ft read, uint32_t size);

static inline uint32_t mmi_write(MMI self, uint32_t address, const void *src, uint32_t size) { return self->write(self, address, src, size); }
static inline uint32_t mmi_read(MMI self, uint32_t address, void *dst, uint32_t size) { return self->read(self, address, dst, size); }
static inline uint32_t mmi_size(MMI self) { return self->size; }

// Copy data from one interface to another.
uint32_t mmi_copy(MMI mmi0, uint32_t addr0, uint32_t amount, MMI mmi1, uint32_t addr1);
