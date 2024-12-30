#pragma once
#include <stdint.h>

#define USB_ALT_INVALID 0xFF

// Set up USB.
void usb_start(uint8_t device, uint32_t n_interfaces);

// Usb interfaces should register a callback that gets called when the configuration changes.
typedef void (*usb_cfg_change_cb)(void);

// Register an interface to update its configuration when it changes.
void usb_register_interface(uint8_t interface, usb_cfg_change_cb cfg_change_cb);
