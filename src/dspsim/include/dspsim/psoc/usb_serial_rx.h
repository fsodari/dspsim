#pragma once
#include "dspsim/psoc/usb_serial.h"
#include "stream_buffer.h"

typedef struct USBSerialRxDef *USBSerialRx;

USBSerialRx usb_serial_rx_start(USBSerial usb_serial, uint32_t rx_buffer_size, uint32_t priority);

StreamBufferHandle_t usb_serial_rx_buf(USBSerialRx self);

void usb_serial_rx_ep_isr(USBSerialRx self);
