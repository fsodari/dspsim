#include "avril.h"
#include <stdlib.h>

void avril_init(Avril avril, VMMI *vmmi, VMMIMeta *vmmi_meta, avril_resp_cb response_cb)
{
    avril->vmmi = vmmi;
    avril->vmmi_meta = vmmi_meta;
    avril->response_cb = response_cb;
}

Avril avril_create(VMMI *vmmi, VMMIMeta *vmmi_meta, avril_resp_cb response_cb)
{
    Avril self = malloc(sizeof(*self));
    avril_init(self, vmmi, vmmi_meta, response_cb);
    return self;
}

struct CmdHeader
{
    uint8_t command;
    uint8_t mode;
    uint16_t msg_id;
    uint32_t size;
    uint32_t address;
};

static uint8_t _static_resp_buf[1024];

uint32_t avril_execute(Avril avril, const uint8_t *buf, uint32_t size)
{
    struct CmdHeader *header = (struct CmdHeader *)buf;
    uint8_t *dbuf = header->command == AVRIL_CMD_READ ? _static_resp_buf : (buf + sizeof(struct CmdHeader));

    MMI *iface = header->mode == AVRIL_MODE_METADATA ? avril->vmmi_meta : avril->vmmi;
    uint32_t error = 0;
    switch (header->command)
    {
    case AVRIL_CMD_WRITE:
        error = mmi_write(iface, header->address, buf + sizeof(struct CmdHeader), header->size);
        break;
    case AVRIL_CMD_READ:
        error = mmi_read(iface, header->address, _static_resp_buf + 16, header->size);
        break;
    }

    struct CmdHeader *resp_header = _static_resp_buf;
    uint32_t *errptr = (uint32_t *)(_static_resp_buf + 12);
    resp_header->address = header->address;
    *errptr = error;

    // Send response
    switch (header->command)
    {
    case AVRIL_CMD_WRITE:
        resp_header->command = AVRIL_CMD_WRITE_ACK;
        resp_header->size = 4;
    case AVRIL_CMD_READ:
        resp_header->command = AVRIL_CMD_READ_ACK;
        resp_header->size = header->size + 4;
        break;
    }
    avril->response_cb(_static_resp_buf, 12 + resp_header->size);
}
