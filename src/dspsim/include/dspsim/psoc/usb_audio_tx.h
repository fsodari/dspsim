#pragma once
#include "dspsim/psoc/usb_core.h"
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "queue.h"
#include "task.h"

typedef struct USBAudioTxDef *USBAudioTx;

// Convert sample rate in Hz to the feedback format.
uint32_t fs_to_feedback(uint32_t sample_rate);
uint32_t feedback_to_fs(uint32_t fb);

//
USBAudioTx usb_audio_tx_start(USBCore usb_core, uint32_t sample_rate, uint8_t interface, uint8_t tx_ep, uint8_t fb_ep, uint32_t tx_ep_buf_size);

MessageBufferHandle_t usb_audio_tx_msg_buf(USBAudioTx self);
QueueHandle_t usb_audio_tx_fb_buf(USBAudioTx self);

// uint32_t usb_audio_tx_update_feedback(USBAudioTx self, uint32_t feedback);
// uint32_t usb_audio_tx_update_sample_rate(USBAudioTx self, uint32_t sample_rate);

// Set these up to get called in the appropriate isr.
void usb_audio_tx_isr(USBAudioTx self);
void usb_audio_fb_isr(USBAudioTx self);
