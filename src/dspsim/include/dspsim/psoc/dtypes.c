#include "dspsim/psoc/dtypes.h"
#include <stdlib.h>

uint32_t dtype_size(DType dtype)
{
    return abs(dtype) & 0xFFFF;
}
