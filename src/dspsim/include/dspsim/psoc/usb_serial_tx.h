#pragma once
#include "dspsim/psoc/usb_serial.h"
#include "stream_buffer.h"

typedef struct USBSerialTxDef *USBSerialTx;

USBSerialTx usb_serial_tx_start(USBSerial usb_serial, uint32_t tx_buffer_size, uint32_t priority);

StreamBufferHandle_t usb_serial_tx_buf(USBSerialTx self);

void usb_serial_tx_notify(USBSerialTx self);

void usb_serial_tx_ep_isr(USBSerialTx self);
