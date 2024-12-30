#pragma once
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include <stdint.h>

// Configure and start up usb serial module.
void usb_serial_start(uint8_t ctl_iface, uint8_t ctl_ep, uint8_t data_iface, uint8_t tx_ep, uint8_t rx_ep, uint32_t tx_buffer_size, uint32_t rx_buffer_size, uint8_t priority);

// Number of bytes waiting to be sent.
uint32_t usb_serial_tx_buf_size(void);

// Erase the tx buf.
uint32_t usb_serial_reset_tx_buf();

// Number of bytes ready to read.
uint32_t usb_serial_rx_buf_size(void);

uint32_t usb_serial_reset_rx_buf();

// Write data into usb send buffer.
uint32_t usb_serial_write(const void *src, uint32_t amount, uint32_t timeout);

// Read data from usb rx buffer.
uint32_t usb_serial_read(void *dst, uint32_t amount, uint32_t timeout);

void usb_serial_tx_ep_isr();
void usb_serial_rx_ep_isr();

StreamBufferHandle_t usb_serial_tx_buf();
StreamBufferHandle_t usb_serial_rx_buf();
