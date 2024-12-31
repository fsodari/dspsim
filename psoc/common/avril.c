#include "FreeRTOS.h"
#include "task.h"

#include "dspsim/avril.h"
#include "dspsim/usb_serial.h"
#include <string.h>

#define MAX_MSG_SIZE 1024

// static StreamBufferHandle_t tx_buffer;
// static StreamBufferHandle_t rx_buffer;
// static StreamBufferHandle_t msg_buffer;

static MessageBufferHandle_t rx_msg_buffer;
static MessageBufferHandle_t tx_msg_buffer;

static uint8_t *msg_buffer;

static MMI **modes;
static uint32_t max_modes = 0;
static uint32_t n_modes = 0;

void AvrilTask(void *arg);

// void avril_start(uint32_t _max_modes, StreamBufferHandle_t _tx_buffer, StreamBufferHandle_t _rx_buffer)
// {
//     tx_buffer = _tx_buffer;
//     rx_buffer = _rx_buffer;
//     max_modes = _max_modes;

//     msg_buffer = pvPortMalloc(MAX_MSG_SIZE);

//     modes = pvPortMalloc(max_modes * sizeof(*modes));
//     n_modes = 0;

//     xTaskCreate(&AvrilTask, "", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
// }
void avril_start(uint32_t _max_modes, MessageBufferHandle_t _tx_buffer, MessageBufferHandle_t _rx_buffer)
{
    tx_msg_buffer = _tx_buffer;
    rx_msg_buffer = _rx_buffer;
    max_modes = _max_modes;

    msg_buffer = pvPortMalloc(MAX_MSG_SIZE);

    modes = pvPortMalloc(max_modes * sizeof(*modes));
    n_modes = 0;

    xTaskCreate(&AvrilTask, "", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
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

static uint32_t nop_ack(CmdHeader *header, uint8_t *dst)
{
    header->command = AVRIL_CMD_NOP_ACK;
    header->size = 0;
    memcpy(dst, header, sizeof(*header));
    return sizeof(CmdHeader);
    // xStreamBufferSend(tx_buffer, header, sizeof(*header), pdMS_TO_TICKS(10));
    // usb_serial_tx_notify();
}
static uint32_t write_ack(CmdHeader *header, uint32_t error, uint8_t *dst)
{
    header->command = AVRIL_CMD_WRITE_ACK;
    header->size = sizeof(error);
    memcpy(dst, header, sizeof(*header));
    dst += sizeof(*header);
    memcpy(dst, &error, sizeof(error));
    return sizeof(CmdHeader) + sizeof(error);
    // xStreamBufferSend(tx_buffer, header, sizeof(*header), pdMS_TO_TICKS(10));
    // xStreamBufferSend(tx_buffer, &error, sizeof(error), pdMS_TO_TICKS(10));
    // // usb_serial_tx_notify();
}

static uint32_t read_ack(CmdHeader *header, uint32_t error, uint8_t *dst)
{
    uint32_t data_size = header->size;

    header->command = AVRIL_CMD_READ_ACK;
    header->size = sizeof(error) + data_size;
    memcpy(dst, header, sizeof(*header));
    dst += sizeof(*header);
    memcpy(dst, &error, sizeof(error));
    return sizeof(CmdHeader) + sizeof(error) + data_size;
    // dst += sizeof(error);
    // memcpy(dst, src, data_size);

    // xStreamBufferSend(tx_buffer, header, sizeof(*header), pdMS_TO_TICKS(10));
    // xStreamBufferSend(tx_buffer, &error, sizeof(error), pdMS_TO_TICKS(10));
    // xStreamBufferSend(tx_buffer, src, data_size, pdMS_TO_TICKS(10));
    // // usb_serial_tx_notify();
}

void AvrilTask(void *arg)
{
    (void)arg;

    uint32_t refresh_timeout = pdMS_TO_TICKS(1);
    uint32_t msg_timeout = pdMS_TO_TICKS(100);
    CmdHeader header;

    for (;;)
    {
        uint32_t received = xMessageBufferReceive(rx_msg_buffer, msg_buffer, MAX_MSG_SIZE, portMAX_DELAY);
        if (received)
        {
            CmdHeader *header = (CmdHeader *)msg_buffer;
            uint8_t *data = msg_buffer + sizeof(CmdHeader);

            uint32_t error = 0;
            MMI *mode = modes[header->mode];
            uint32_t response_size = 0;
            switch (header->command)
            {
            case AVRIL_CMD_NOP:
                response_size = nop_ack(header, msg_buffer);
                break;
            case AVRIL_CMD_WRITE:
                if (header->size == (received - sizeof(CmdHeader)))
                {
                    error = mmi_write(mode, header->address, data, header->size);
                }
                else
                {
                    error = 1;
                }
                response_size = write_ack(header, error, msg_buffer);

                break;
            case AVRIL_CMD_READ:
                error = mmi_read(mode, header->address, data + 4, header->size);
                response_size = read_ack(header, error, msg_buffer);
                break;
            default:
                break;
            }
            if (response_size)
            {
                xMessageBufferSend(tx_msg_buffer, msg_buffer, response_size, portMAX_DELAY);
            }
        }
        // uint32_t count = xStreamBufferBytesAvailable(rx_buffer);
        // if (count >= sizeof(CmdHeader))
        // {
        //     count = xStreamBufferReceive(rx_buffer, &header, sizeof(header), 0);
        // }
        // else
        // {
        //     vTaskDelay(refresh_timeout);
        // }
        // // // Receive the header.
        // // uint32_t count = xStreamBufferReceive(rx_buffer, &header, sizeof(header), refresh_timeout);
        // if (count == sizeof(header))
        // {
        //     // Receive the rest of the message.
        //     uint32_t received = 0;
        //     if ((header.command == AVRIL_CMD_WRITE) && header.size)
        //     {
        //         while (xStreamBufferBytesAvailable(rx_buffer) < header.size)
        //         {
        //             vTaskDelay(pdMS_TO_TICKS(1));
        //         }
        //         received = xStreamBufferReceive(rx_buffer, msg_buffer, header.size, msg_timeout);
        //     }
        //     uint32_t error = 0;
        //     MMI *mode = modes[header.mode];

        //     switch (header.command)
        //     {
        //     case AVRIL_CMD_NOP:
        //         nop_ack(&header);
        //         break;
        //     case AVRIL_CMD_WRITE:
        //         if (received == header.size)
        //         {
        //             error = mmi_write(mode, header.address, msg_buffer, header.size);
        //             write_ack(&header, error);
        //         }
        //         else
        //         {
        //             // send ack with error.
        //         }
        //         break;
        //     case AVRIL_CMD_READ:
        //         error = mmi_read(mode, header.address, msg_buffer, header.size);
        //         read_ack(&header, error, msg_buffer);
        //         break;
        //     default:
        //         break;
        //     }
        // }
    }
}
