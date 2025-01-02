#include "portable.h"
#include <stdlib.h>

void *pvPortMalloc(size_t xSize)
{
    return malloc(xSize);
}

void *pvPortCalloc(size_t xNum, size_t xSize)
{
    return calloc(xNum, xSize);
}

void vPortFree(void *pv)
{
    free(pv);
}
