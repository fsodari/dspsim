
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "dspsim/usb_serial.h"
#include "dspsim/usb_core.h"

#include <USBFS.h>
#include <USBFS_cdc.h>
#include <stdint.h>

StreamBufferHandle_t tx_buffer = NULL;
StreamBufferHandle_t rx_buffer = NULL;

static uint8_t tx_ep_buf[64];
static uint8_t rx_ep_buf[64];

static uint8_t _ctl_ep = 0;
static uint8_t _tx_ep = 0;
static uint8_t _rx_ep = 0;

void usb_serial_ctl_cfg_change_cb() {}
void usb_serial_data_cfg_change_cb()
{
    USBFS_LoadInEP(_tx_ep, tx_ep_buf, sizeof(tx_ep_buf));
    USBFS_ReadOutEP(_rx_ep, rx_ep_buf, sizeof(rx_ep_buf));
    USBFS_EnableOutEP(_rx_ep);
}

// USB Serial Tx Task.
void USBSerialTx(void *arg);
// USB Serial Rx Task.
void USBSerialRx(void *arg);

static TaskHandle_t serial_tx_task = NULL, serial_rx_task = NULL;

void usb_serial_start(
    uint8_t ctl_iface,
    uint8_t ctl_ep,
    uint8_t data_iface,
    uint8_t tx_ep,
    uint8_t rx_ep,
    uint32_t tx_buffer_size,
    uint32_t rx_buffer_size,
    uint8_t priority)
{
    tx_buffer = xStreamBufferCreate(tx_buffer_size, 1);
    rx_buffer = xStreamBufferCreate(rx_buffer_size, 1);

    _ctl_ep = ctl_ep;
    _tx_ep = tx_ep;
    _rx_ep = rx_ep;

    usb_register_interface(ctl_iface, usb_serial_ctl_cfg_change_cb);
    usb_register_interface(data_iface, usb_serial_data_cfg_change_cb);

    USBFS_CDC_Init();

    xTaskCreate(&USBSerialTx, "", configMINIMAL_STACK_SIZE, NULL, priority, &serial_tx_task);
    xTaskCreate(&USBSerialRx, "", configMINIMAL_STACK_SIZE, NULL, priority, &serial_rx_task);
}

// Number of bytes waiting to be sent.
uint32_t usb_serial_tx_buf_size(void)
{
    return xStreamBufferBytesAvailable(tx_buffer);
}

// Erase the tx buf.
uint32_t usb_serial_reset_tx_buf()
{
    return xStreamBufferReset(tx_buffer);
}

// Number of bytes ready to read.
uint32_t usb_serial_rx_buf_size(void)
{
    return xStreamBufferBytesAvailable(rx_buffer);
}

// Erase the tx buf.
uint32_t usb_serial_reset_rx_buf()
{
    return xStreamBufferReset(rx_buffer);
}

// Write data into usb send buffer.
uint32_t usb_serial_write(const void *src, uint32_t amount, uint32_t timeout)
{
    return xStreamBufferSend(tx_buffer, src, amount, timeout);
}

void usb_serial_tx_notify()
{
    xTaskNotifyGive(serial_tx_task);
}

// Read data from usb rx buffer.
uint32_t usb_serial_read(void *dst, uint32_t amount, uint32_t timeout)
{
    return xStreamBufferReceive(rx_buffer, dst, amount, timeout);
}

StreamBufferHandle_t usb_serial_tx_buf()
{
    return tx_buffer;
}
StreamBufferHandle_t usb_serial_rx_buf()
{
    return rx_buffer;
}

void USBSerialTx(void *arg)
{
    (void)arg;

    uint32_t timeout = pdMS_TO_TICKS(4);
    for (;;)
    {
        // Refresh or wait on isr to unblock.

        if (USBFS_GetEPState(_tx_ep) == USBFS_IN_BUFFER_EMPTY)
        {
            uint32_t received = xStreamBufferReceive(tx_buffer, tx_ep_buf, 64, timeout);
            if (received)
            {
                USBFS_LoadInEP(_tx_ep, NULL, received);
            }
        }
        else
        {
            ulTaskNotifyTake(pdTRUE, timeout);
        }
    }
}

void usb_serial_tx_ep_isr()
{
    BaseType_t awoken_task = pdFALSE;
    vTaskNotifyGiveFromISR(serial_tx_task, &awoken_task);
    portYIELD_FROM_ISR(awoken_task);
}

void USBSerialRx(void *arg)
{
    (void)arg;
    uint32_t timeout = pdMS_TO_TICKS(4);

    for (;;)
    {
        // Data is available.
        if (ulTaskNotifyTake(pdTRUE, timeout))
        {
            if (USBFS_GetEPState(_rx_ep))
            {
                uint32_t count = USBFS_GetEPCount(_rx_ep);
                if (count)
                {
                    // send data from ep buf to rx_buffer stream.
                    xStreamBufferSend(rx_buffer, rx_ep_buf, count, portMAX_DELAY);
                    USBFS_EnableOutEP(_rx_ep);
                }
            }
        }
    }
}

void usb_serial_rx_ep_isr()
{
    BaseType_t awoken_task = pdFALSE;
    vTaskNotifyGiveFromISR(serial_rx_task, &awoken_task);
    portYIELD_FROM_ISR(awoken_task);
}
