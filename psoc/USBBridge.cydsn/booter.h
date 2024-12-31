#pragma once
#include "dspsim/mmi.h"
#include "timers.h"

typedef struct BooterDef *Booter;
struct BooterDef
{
    struct MMIDef base;
    uint32_t password;
    TimerHandle_t delay_timer;
};

Booter booter_create(uint32_t password);
void booter_set_password(Booter self, uint32_t password);
