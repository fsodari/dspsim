#include "FreeRTOS.h"
#include "task.h"
#include "dspsim/psoc/usb_core.h"

#include <USBFS.h>
#include <USBFS_cdc.h>

#define USB_ALT_INVALID 0xFF

typedef struct USBCfgDef *USBCfg;
struct USBCfgDef
{
    void *iface;
    uint8_t id;
    uint8_t settings;
    usb_cfg_change_cb callback;
};

struct USBCoreDef
{
    USBCfg interfaces;
    uint32_t max_interfaces;
    uint32_t n_interfaces;
    TaskHandle_t task_ref;
};

// Handle usb configuration when it changes.
void USBConfigService(void *_self);

USBCore usb_start(uint8_t device, uint32_t max_interfaces)
{
    USBCore self = pvPortMalloc(sizeof(*self));
    self->max_interfaces = max_interfaces;
    // Reserve space for interfaces
    self->interfaces = pvPortMalloc(max_interfaces * sizeof(*self->interfaces));
    self->n_interfaces = 0;

    USBFS_Start(device, USBFS_DWR_VDDD_OPERATION);

    xTaskCreate(&USBConfigService, "", configMINIMAL_STACK_SIZE, self, 3, &self->task_ref);
    return self;
}

void usb_register_interface(USBCore self, void *_iface, uint8_t interface, usb_cfg_change_cb cfg_change_cb)
{
    if (self->n_interfaces < self->max_interfaces)
    {
        // Append the interface to the list.
        self->interfaces[self->n_interfaces++] = (struct USBCfgDef){_iface, interface, USB_ALT_INVALID, cfg_change_cb};
    }
}

// Fun fun usb stuff.
void USBConfigService(void *_self)
{
    USBCore self = _self;

    const TickType_t RefreshDelay = pdMS_TO_TICKS(50);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Start USB.

    //
    while (!USBFS_GetConfiguration())
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (;;)
    {
        if (USBFS_IsConfigurationChanged())
        {
            for (uint32_t i = 0; i < self->n_interfaces; i++)
            {
                USBCfg iface = &self->interfaces[i];
                uint8_t new_setting = USBFS_GetInterfaceSetting(iface->id);
                // Configuration changed, call the callback.
                if (iface->settings != new_setting)
                {
                    iface->settings = new_setting;
                    // Call the callback.
                    iface->callback(iface->iface);
                }
            }
        }
        vTaskDelayUntil(&xLastWakeTime, RefreshDelay);
    }
}
