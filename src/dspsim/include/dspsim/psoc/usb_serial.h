#pragma once
#include "dspsim/psoc/usb_core.h"

typedef struct USBSerialDef *USBSerial;

// Configure and start up usb serial module.
USBSerial usb_serial_start(USBCore usb_core, uint8_t ctl_iface, uint8_t ctl_ep, uint8_t data_iface, uint8_t tx_ep, uint8_t rx_ep);
