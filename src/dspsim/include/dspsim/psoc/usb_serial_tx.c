#include "dspsim/psoc/usb_serial_tx.h"
#include "usb_serial_pvt.h"
#include <USBFS.h>

struct USBSerialTxDef
{
    uint8_t tx_ep;
    uint8_t *tx_ep_buf;
    StreamBufferHandle_t tx_buf;
    TaskHandle_t tx_task;
};

// USB Serial Tx Task.
void USBSerialTxTask(void *arg);

USBSerialTx usb_serial_tx_start(USBSerial usb_serial, uint32_t tx_buffer_size, uint32_t priority)
{
    USBSerialTx self = pvPortMalloc(sizeof(*self));

    self->tx_ep = usb_serial->tx_ep;
    self->tx_ep_buf = usb_serial->tx_ep_buf;

    self->tx_buf = xStreamBufferCreate(tx_buffer_size, 64);

    xTaskCreate(&USBSerialTxTask, "", configMINIMAL_STACK_SIZE, self, priority, &self->tx_task);

    return self;
}

StreamBufferHandle_t usb_serial_tx_buf(USBSerialTx self)
{
    return self->tx_buf;
}

void usb_serial_tx_notify(USBSerialTx self)
{
    xTaskNotifyGive(self->tx_task);
}

void USBSerialTxTask(void *_self)
{
    USBSerialTx self = _self;

    uint32_t timeout = pdMS_TO_TICKS(1);
    for (;;)
    {
        // Refresh or wait on isr to unblock.
        if (USBFS_GetEPState(self->tx_ep) == USBFS_IN_BUFFER_EMPTY)
        {
            uint32_t received = xStreamBufferReceive(self->tx_buf, self->tx_ep_buf, 64, timeout);
            if (received)
            {
                USBFS_LoadInEP(self->tx_ep, NULL, received);
                uint32_t remaining = xStreamBufferBytesAvailable(self->tx_buf);

                // If it's exactly 64 bytes
                if (received == 64 && remaining == 0)
                {
                    while (!USBFS_GetEPState(self->tx_ep))
                    {
                    }
                    USBFS_LoadInEP(self->tx_ep, NULL, 0);
                }
            }
        }
        else
        {
            ulTaskNotifyTake(pdTRUE, timeout);
        }
    }
}

void usb_serial_tx_ep_isr(USBSerialTx self)
{
    BaseType_t awoken_task = pdFALSE;
    vTaskNotifyGiveFromISR(self->tx_task, &awoken_task);
    portYIELD_FROM_ISR(awoken_task);
}