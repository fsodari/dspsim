#include "dspsim/mmi.h"

void mmi_init(MMI *self, mmi_write_ft write, mmi_read_ft read, uint32_t size)
{
    self->write = write;
    self->read = read;
    self->size = size;
}