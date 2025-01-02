/*

    Memory mapped interface

*/
#pragma once
#include "dspsim/psoc/error_codes.h"
#include "dspsim/psoc/dtypes.h"
#include <stdint.h>

extern uint8_t *mmi_swap_buf;
uint32_t mmi_dtype_size(uint32_t dtype);

// Interfaces must implement these function types.
typedef uint32_t (*mmi_write_ft)(void *self, uint32_t address, const void *src, uint32_t size);
typedef uint32_t (*mmi_read_ft)(void *self, uint32_t address, void *dst, uint32_t size);

typedef struct MIterDef *MIter;
typedef void (*miter_next_ft)(MIter);

typedef struct MMIDef *MMI;
struct MMIDef
{
    mmi_write_ft write;
    mmi_read_ft read;
    uint32_t size;
    DType dtype;
    miter_next_ft next;
};

void mmi_init(MMI self, mmi_write_ft write, mmi_read_ft read, uint32_t size, DType dtype);

static inline uint32_t mmi_write(MMI self, uint32_t address, const void *src, uint32_t size) { return self->write(self, address, src, size); }
static inline uint32_t mmi_read(MMI self, uint32_t address, void *dst, uint32_t size) { return self->read(self, address, dst, size); }
static inline uint32_t mmi_size(MMI self) { return self->size; }

static inline uint32_t mmi_write_reg(MMI self, uint32_t address, const void *src) { return mmi_write(self, address, src, mmi_dtype_size(self->dtype)); }
static inline uint32_t mmi_read_reg(MMI self, uint32_t address, void *dst) { return mmi_read(self, address, dst, mmi_dtype_size(self->dtype)); }

// Copy data from one interface to another.
uint32_t mmi_copy(MMI mmi0, uint32_t addr0, uint32_t amount, MMI mmi1, uint32_t addr1);

// Standardized functions that mmis can use.
uint32_t mmi_fread_only_err(void *self, uint32_t address, const void *src, uint32_t size);
uint32_t mmi_fwrite_only_err(void *self, uint32_t address, void *dst, uint32_t size);

// Utilities
uint32_t mmi_check_address_align(uint32_t address, uint32_t required_alignment);
uint32_t mmi_check_size_align(uint32_t size, uint32_t required_alignment);

uint32_t mmi_check_align(uint32_t address, uint32_t size, uint32_t required_alignment);

uint32_t mmi_check_overflow(MMI self, uint32_t address, uint32_t size);
uint32_t mmi_check_overflow_size(uint32_t address, uint32_t size, uint32_t interface_size);
