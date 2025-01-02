#pragma once
#include <stddef.h>

void *pvPortMalloc(size_t xSize);
void *pvPortCalloc(size_t xNum, size_t xSize);
void vPortFree(void *pv);
