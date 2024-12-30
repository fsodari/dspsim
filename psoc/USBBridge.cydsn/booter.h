#pragma once
#include "dspsim/mmi.h"

extern MMI *booter_mmi;

void booter_start(void);
void booter_set_password(uint32_t password);
