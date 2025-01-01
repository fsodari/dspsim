#include "dspsim/psoc/usb_audio_tx.h"
#include <USBFS.h>

struct USBAudioTxDef
{
    // Audio messages are written into this buffer. Downstream tasks should wait on messages from this buffer.
    MessageBufferHandle_t msg_buf;

    // Feedback updates should be written into this queue
    QueueHandle_t fb_buf;

    // Audio data endpoint.
    uint8_t tx_ep;
    // Feedback endpoint.
    uint8_t fb_ep;

    // Data endpoint buffer.
    uint8_t *tx_ep_buf;
    uint32_t tx_ep_buf_size;

    // Feedback buffer is always 3 bytes.
    uint8_t fb_ep_buf[3];

    // Byte swap buffer. Same size as endpoint buffer. Data is endian swapped here before transmitting to msg buf.
    uint8_t *byte_swap_buf;
    TaskHandle_t byte_swap_task;
};

static inline void unpack_fb(USBAudioTx self, uint32_t feedback)
{
    self->fb_ep_buf[2] = (feedback >> 16) & 0xFF;
    self->fb_ep_buf[1] = (feedback >> 8) & 0xFF;
    self->fb_ep_buf[0] = feedback & 0xFF;
}

void usb_audio_tx_cfg_change_cb(void *_self)
{
    USBAudioTx self = _self;

    USBFS_ReadOutEP(self->tx_ep, self->tx_ep_buf, self->tx_ep_buf_size);
    USBFS_EnableOutEP(self->tx_ep);
    USBFS_LoadInEP(self->fb_ep, self->fb_ep_buf, sizeof(self->fb_ep_buf));
    USBFS_LoadInEP(self->fb_ep, NULL, 3);
}

void AudioTxByteSwapTask(void *_self);

USBAudioTx usb_audio_tx_start(USBCore usb_core, uint32_t sample_rate, uint8_t interface, uint8_t tx_ep, uint8_t fb_ep, uint32_t tx_ep_buf_size)
{
    USBAudioTx self = pvPortMalloc(sizeof(*self));
    self->msg_buf = xMessageBufferCreate(tx_ep_buf_size + 4);
    self->fb_buf = xQueueCreate(1, sizeof(uint32_t));
    self->tx_ep = tx_ep;
    self->fb_ep = fb_ep;
    self->tx_ep_buf = pvPortMalloc(tx_ep_buf_size);
    self->tx_ep_buf_size = tx_ep_buf_size;
    self->byte_swap_buf = pvPortMalloc(tx_ep_buf_size);

    // Initialize feedback
    uint32_t feedback = fs_to_feedback(sample_rate);
    unpack_fb(self, feedback);

    usb_register_interface(usb_core, self, interface, usb_audio_tx_cfg_change_cb);

    xTaskCreate(&AudioTxByteSwapTask, "", configMINIMAL_STACK_SIZE, self, 2, &self->byte_swap_task);

    return self;
}
MessageBufferHandle_t usb_audio_tx_msg_buf(USBAudioTx self)
{
    return self->msg_buf;
}

QueueHandle_t usb_audio_tx_fb_buf(USBAudioTx self)
{
    return self->fb_buf;
}

uint32_t fs_to_feedback(uint32_t sample_rate)
{
    return (16384 * sample_rate) / 1000;
}
uint32_t feedback_to_fs(uint32_t fb)
{
    return (fb * 1000) / 16384;
}

static inline void byte_swap24(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[2];
    // dst[1] = src[1];
    dst[2] = src[0];
}

static inline void byte_swap24_all(uint8_t *dst, const uint8_t *src, uint32_t size)
{
    for (uint32_t i = 0; i < size; i += 6)
    {
        byte_swap24(&dst[i], &src[i]);
        byte_swap24(&dst[i + 3], &src[i + 3]);
    }
}

void AudioTxByteSwapTask(void *_self)
{
    USBAudioTx self = _self;
    uint32_t count = 0;

    for (;;)
    {
        if (xTaskNotifyWait(0, UINT32_MAX, &count, portMAX_DELAY))
        {
            // Swap the bytes.
            byte_swap24_all(self->byte_swap_buf, self->tx_ep_buf, count);
            // Send to message buffer
            xMessageBufferSend(self->msg_buf, self->byte_swap_buf, count, portMAX_DELAY);
        }
    }
}

// Set these up to get called in the appropriate isr.
void usb_audio_tx_isr(USBAudioTx self)
{
    BaseType_t awoken = pdFALSE;
    // Get the number of bytes transferred.
    uint32_t count = USBFS_GetEPCount(self->tx_ep);
    // // Send to the message buffer.
    // xMessageBufferSendFromISR(self->msg_buf, self->tx_ep_buf, count, &awoken);

    // Notify byte swap task.
    xTaskNotifyFromISR(self->byte_swap_task, count, eSetValueWithOverwrite, &awoken);

    portYIELD_FROM_ISR(awoken);
}

void usb_audio_fb_isr(USBAudioTx self)
{
    BaseType_t awoken = pdFALSE;
    uint32_t feedback = 0;
    if (xQueueReceiveFromISR(self->fb_buf, &feedback, &awoken))
    {
        unpack_fb(self, feedback);
    }

    // Write data to the feedback endpoint.
    if (USBFS_GetEPState(self->fb_ep))
    {
        USBFS_LoadInEP(self->fb_ep, NULL, 3);
    }
    portYIELD_FROM_ISR(awoken);
}
