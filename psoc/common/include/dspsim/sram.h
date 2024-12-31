#pragma once
#include "dspsim/mmi.h"

typedef struct SramDef *Sram;
struct SramDef
{
    struct MMIDef base;
    uint8_t *buf;
    uint32_t size;
};

Sram sram_create(uint32_t size, int32_t dtype);
