#pragma once
#include <stdint.h>

typedef int32_t (*avril_write_ft)(uint32_t, const void *, uint32_t);
typedef int32_t (*avril_read_ft)(uint32_t, void *dst, uint32_t);

typedef struct AvrilNodeDef *AvrilNode;
struct AvrilNodeDef
{
    avril_write_ft write_func;
    avril_read_ft read_func;
};

typedef struct AvrilDef *Avril;

struct AvrilDef
{
};

Avril avril_create();
