#pragma once
#include "dspsim/psoc/mmi.h"
#include "timers.h"

typedef struct BooterDef *Booter;
struct BooterDef
{
    struct MMIDef base;
    int32_t password;
    TimerHandle_t delay_timer;
};

Booter booter_create(int32_t password);
void booter_set_password(Booter self, int32_t password);
