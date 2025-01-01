#include "dspsim/psoc/usb_serial_rx.h"
#include "usb_serial_pvt.h"
#include <USBFS.h>

struct USBSerialRxDef
{
    uint8_t rx_ep;
    uint8_t *rx_ep_buf;
    StreamBufferHandle_t rx_buf;
    TaskHandle_t rx_task;
};

void USBSerialRxTask(void *_self);

USBSerialRx usb_serial_rx_start(USBSerial usb_serial, uint32_t rx_buffer_size, uint32_t priority)
{
    USBSerialRx self = pvPortMalloc(sizeof(*self));
    self->rx_ep = usb_serial->rx_ep;
    self->rx_ep_buf = usb_serial->rx_ep_buf;

    self->rx_buf = xStreamBufferCreate(rx_buffer_size, 1);

    xTaskCreate(&USBSerialRxTask, "", configMINIMAL_STACK_SIZE, self, priority, &self->rx_task);

    return self;
}

StreamBufferHandle_t usb_serial_rx_buf(USBSerialRx self)
{
    return self->rx_buf;
}

void USBSerialRxTask(void *_self)
{
    USBSerialRx self = _self;
    uint32_t timeout = pdMS_TO_TICKS(1);

    for (;;)
    {
        // Data is available.
        if (ulTaskNotifyTake(pdTRUE, timeout))
        {
            if (USBFS_GetEPState(self->rx_ep))
            {
                uint32_t count = USBFS_GetEPCount(self->rx_ep);
                if (count)
                {
                    // send data from ep buf to rx_buffer stream.
                    xStreamBufferSend(self->rx_buf, self->rx_ep_buf, count, portMAX_DELAY);
                    USBFS_EnableOutEP(self->rx_ep);
                }
            }
        }
    }
}

void usb_serial_rx_ep_isr(USBSerialRx self)
{
    BaseType_t awoken_task = pdFALSE;
    vTaskNotifyGiveFromISR(self->rx_task, &awoken_task);
    portYIELD_FROM_ISR(awoken_task);
}
