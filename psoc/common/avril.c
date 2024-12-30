#include "FreeRTOS.h"
#include "task.h"

#include "dspsim/avril.h"

#include <string.h>

#define MAX_MSG_SIZE 256

static StreamBufferHandle_t tx_buffer;
static StreamBufferHandle_t rx_buffer;
// static StreamBufferHandle_t msg_buffer;
static uint8_t *msg_buffer;

static MMI **modes;
static uint32_t max_modes = 0;
static uint32_t n_modes = 0;

void AvrilTask(void *arg);

void avril_start(uint32_t _max_modes, StreamBufferHandle_t _tx_buffer, StreamBufferHandle_t _rx_buffer)
{
    tx_buffer = _tx_buffer;
    rx_buffer = _rx_buffer;
    max_modes = _max_modes;

    msg_buffer = pvPortMalloc(MAX_MSG_SIZE);

    modes = pvPortMalloc(max_modes * sizeof(*modes));
    n_modes = 0;

    xTaskCreate(&AvrilTask, "", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
}

void avril_add_mode(uint32_t mode_id, MMI *mode_interface)
{
    modes[mode_id] = mode_interface;
}

typedef struct CmdHeader
{
    uint8_t command;
    uint8_t mode;
    uint16_t msg_id;
    uint32_t size;
    uint32_t address;
} CmdHeader;

static void nop_ack(CmdHeader *header)
{
    header->command = AVRIL_CMD_NOP_ACK;
    header->size = 0;
    xStreamBufferSend(tx_buffer, header, sizeof(*header), pdMS_TO_TICKS(100));
}
static void write_ack(CmdHeader *header, uint32_t error)
{
    header->command = AVRIL_CMD_WRITE_ACK;
    header->size = sizeof(error);
    xStreamBufferSend(tx_buffer, header, sizeof(*header), pdMS_TO_TICKS(10));
    xStreamBufferSend(tx_buffer, &error, sizeof(error), pdMS_TO_TICKS(10));
}

static void read_ack(CmdHeader *header, uint32_t error, const void *src)
{
    header->command = AVRIL_CMD_READ_ACK;
    header->size = sizeof(error) + header->size;
    xStreamBufferSend(tx_buffer, header, sizeof(*header), pdMS_TO_TICKS(10));
    xStreamBufferSend(tx_buffer, &error, sizeof(error), pdMS_TO_TICKS(10));
    xStreamBufferSend(tx_buffer, src, header->size, pdMS_TO_TICKS(10));
}

void AvrilTask(void *arg)
{
    (void)arg;

    uint32_t refresh_timeout = pdMS_TO_TICKS(10);
    uint32_t msg_timeout = pdMS_TO_TICKS(500);

    for (;;)
    {
        uint32_t count = xStreamBufferBytesAvailable(rx_buffer);
        if (count >= sizeof(CmdHeader))
        {
            // Receive the header.
            CmdHeader header;
            xStreamBufferReceive(rx_buffer, &header, sizeof(header), pdMS_TO_TICKS(10));

            // Receive the rest of the message.
            uint32_t received = header.size ? xStreamBufferReceive(rx_buffer, msg_buffer, header.size, msg_timeout) : 0;
            if (received == header.size)
            {
                uint32_t error = 0;
                MMI *mode = modes[header.mode];

                switch (header.command)
                {
                case AVRIL_CMD_NOP:
                    nop_ack(&header);
                    break;
                case AVRIL_CMD_WRITE:
                    error = mmi_write(mode, header.address, msg_buffer, header.size);
                    write_ack(&header, error);
                    break;
                case AVRIL_CMD_READ:
                    error = mmi_read(mode, header.address, msg_buffer, header.size);
                    read_ack(&header, error, msg_buffer);
                    break;
                default:
                    break;
                }
            }
        }
        else
        {
            vTaskDelay(refresh_timeout);
        }
    }
}
