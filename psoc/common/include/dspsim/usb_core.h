#pragma once
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

// Usb interfaces should register a callback that gets called when the configuration changes.
typedef void (*usb_cfg_change_cb)(void *);

typedef struct USBCoreDef *USBCore;

// Set up USB.
USBCore usb_start(uint8_t device, uint32_t n_interfaces);

// Register an interface to update its configuration when it changes.
void usb_register_interface(USBCore self, void *iface, uint8_t id, usb_cfg_change_cb cfg_change_cb);
