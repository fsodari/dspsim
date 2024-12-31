#pragma once
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#define USB_ALT_INVALID 0xFF

// Usb interfaces should register a callback that gets called when the configuration changes.
typedef void (*usb_cfg_change_cb)(void *);

typedef struct USBCfgDef *USBCfg;
struct USBCfgDef
{
    void *iface;
    uint8_t id;
    uint8_t settings;
    usb_cfg_change_cb callback;
};

typedef struct USBCoreDef *USBCore;
struct USBCoreDef
{
    USBCfg interfaces;
    uint32_t max_interfaces;
    uint32_t n_interfaces;
    TaskHandle_t task_ref;
};

// Set up USB.
USBCore usb_start(uint8_t device, uint32_t n_interfaces);

// Register an interface to update its configuration when it changes.
void usb_register_interface(USBCore self, void *iface, uint8_t id, usb_cfg_change_cb cfg_change_cb);
