#include "dspsim/psoc/avril.h"
#include "dspsim/psoc/avril_msg.h"
#include "dspsim/psoc/usb_serial.h"
#include "dspsim/psoc/cdict.h"

#include <string.h>

struct AvrilDef
{
    MessageBufferHandle_t tx_msg_buf;
    MessageBufferHandle_t rx_msg_buf;
    uint32_t max_msg_size;
    uint32_t max_modes;
    uint32_t n_modes;
    Dict modes;
    uint8_t *msg_buf;
    TaskHandle_t task_ref;
};

void AvrilTask(void *_self);

Avril avril_start(uint32_t max_modes, uint32_t max_msg_size, uint32_t priority)
{
    Avril self = pvPortMalloc(sizeof(*self));
    self->tx_msg_buf = xMessageBufferCreate(max_msg_size);
    self->rx_msg_buf = xMessageBufferCreate(max_msg_size);
    self->max_msg_size = max_msg_size;
    self->max_modes = max_modes;
    self->n_modes = 0;

    self->modes = dict_createl(8);
    self->msg_buf = pvPortMalloc(max_msg_size);

    xTaskCreate(&AvrilTask, "", configMINIMAL_STACK_SIZE, self, priority, &self->task_ref);
    return self;
}

dErrorCodes avril_add_mode(Avril self, uint32_t mode_id, MMI mode_interface)
{
    // self->modes[mode_id] = mode_interface;
    dErrorCodes error = dict_set(self->modes, &mode_id, &mode_interface, sizeof(mode_interface));
    return error;
}

MessageBufferHandle_t avril_tx_msg_buf(Avril self)
{
    return self->tx_msg_buf;
}
MessageBufferHandle_t avril_rx_msg_buf(Avril self)
{
    return self->rx_msg_buf;
}

typedef struct CmdHeader
{
    uint8_t command;
    uint8_t mode;
    uint16_t msg_id;
    uint32_t size;
    uint32_t address;
} CmdHeader;

static uint32_t nop_ack(CmdHeader *header, uint32_t error, uint8_t *dst)
{
    (void)error;
    
    header->command = AvrilNopAck;
    header->size = 0;
    memcpy(dst, header, sizeof(*header));
    return sizeof(CmdHeader);
}

static uint32_t write_ack(CmdHeader *header, uint32_t error, uint8_t *dst)
{
    header->command = AvrilWriteAck;
    header->size = sizeof(error);
    memcpy(dst, header, sizeof(*header));
    dst += sizeof(*header);
    memcpy(dst, &error, sizeof(error));
    return sizeof(CmdHeader) + sizeof(error);
}

static uint32_t read_ack(CmdHeader *header, uint32_t error, uint8_t *dst)
{
    uint32_t data_size = error == dErrNone ? header->size : 0;

    header->command = AvrilReadAck;
    header->size = sizeof(error) + data_size;

    memcpy(dst, header, sizeof(*header));
    dst += sizeof(*header);
    memcpy(dst, &error, sizeof(error));
    return sizeof(CmdHeader) + sizeof(error) + data_size;
}

uint32_t _ack(CmdHeader *header, uint32_t error, uint8_t *dst)
{
    if (header->command == AvrilNop)
    {
        return nop_ack(header, error, dst);
    }
    else if (header->command == AvrilWrite)
    {
        return write_ack(header, error, dst);
    }
    else if (header->command == AvrilRead)
    {
        return read_ack(header, error, dst);
    } else {
        return 0;
    }
}

void AvrilTask(void *_self)
{
    Avril self = _self;

    for (;;)
    {
        uint32_t received = xMessageBufferReceive(self->rx_msg_buf, self->msg_buf, self->max_msg_size, portMAX_DELAY);
        if (received)
        {
            CmdHeader *header = (CmdHeader *)self->msg_buf;
            uint8_t *data = self->msg_buf + sizeof(CmdHeader);

            uint32_t error = 0;
            // MMI mode = self->modes[header->mode];
            MMI mode;
            dict_getl(self->modes, header->mode, &mode);
            
            uint32_t response_size = 0;
            switch (header->command)
            {
            case AvrilNop:
                response_size = nop_ack(header, 0, self->msg_buf);
                break;
            case AvrilWrite:
                if (header->size == (received - sizeof(CmdHeader)))
                {
                    error = mmi_write(mode, header->address, data, header->size);
                }
                else
                {
                    error = 1;
                }
                response_size = write_ack(header, error, self->msg_buf);

                break;
            case AvrilRead:
                error = mmi_read(mode, header->address, data + 4, header->size);
                response_size = read_ack(header, error, self->msg_buf);
                break;
            default:
                break;
            }
            if (response_size)
            {
                xMessageBufferSend(self->tx_msg_buf, self->msg_buf, response_size, portMAX_DELAY);
            }
        }
    }
}
