/*

    Memory mapped interface

*/
#pragma once
#include "dspsim/error_codes.h"
#include <stdint.h>
#include <stdlib.h>

typedef enum MMIDtypes
{
    MMI_x = 0,
    MMI_b = -1,
    MMI_B = 1,
    MMI_h = -2,
    MMI_H = 2,
    MMI_l = 4,
    MMI_L = 4,
    MMI_q = -8,
    MMI_Q = 8,
    MMI_f = 0x14,
    MMI_d = 0x18,
} MMIDtypes;

extern uint8_t *mmi_swap_buf;
static inline uint32_t mmi_dtype_size(int32_t dtype) { return abs(dtype) & 0xF; }

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
    int32_t dtype;
    miter_next_ft next;
};

void mmi_init(MMI self, mmi_write_ft write, mmi_read_ft read, uint32_t size, int32_t dtype);
#define mmi_initx(self, write, read, size) mmi_init(self, write, read, size, MMI_x);
#define mmi_initb(self, write, read, size) mmi_init(self, write, read, size, MMI_b);
#define mmi_initB(self, write, read, size) mmi_init(self, write, read, size, MMI_B);
#define mmi_inith(self, write, read, size) mmi_init(self, write, read, size, MMI_h);
#define mmi_initH(self, write, read, size) mmi_init(self, write, read, size, MMI_H);
#define mmi_initl(self, write, read, size) mmi_init(self, write, read, size, MMI_l);
#define mmi_initL(self, write, read, size) mmi_init(self, write, read, size, MMI_L);
#define mmi_initq(self, write, read, size) mmi_init(self, write, read, size, MMI_q);
#define mmi_initQ(self, write, read, size) mmi_init(self, write, read, size, MMI_Q);
#define mmi_initf(self, write, read, size) mmi_init(self, write, read, size, MMI_f);
#define mmi_initd(self, write, read, size) mmi_init(self, write, read, size, MMI_d);

static inline uint32_t mmi_write(MMI self, uint32_t address, const void *src, uint32_t size) { return self->write(self, address, src, size); }
static inline uint32_t mmi_read(MMI self, uint32_t address, void *dst, uint32_t size) { return self->read(self, address, dst, size); }
static inline uint32_t mmi_size(MMI self) { return self->size; }

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
