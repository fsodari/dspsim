#include "FreeRTOS.h"
#include "task.h"
#include "dspsim/usb_core.h"

#include <USBFS.h>
#include <USBFS_cdc.h>

typedef struct IFaceCfgDef *IFaceCfg;
struct IFaceCfgDef
{
    uint8_t id;
    uint8_t settings;
    usb_cfg_change_cb callback;
};

// Array of registered interfaces. Need to update when configuration changes.
static IFaceCfg interfaces;
static int max_interfaces = 0;
static int n_interfaces = 0;

// Handle usb configuration when it changes.
void USBConfigService(void *arg);

void usb_start(uint8_t device, uint32_t _max_interfaces)
{
    max_interfaces = _max_interfaces;
    
    // Reserve space for interfaces
    interfaces = pvPortMalloc(max_interfaces * sizeof(*interfaces));
    n_interfaces = 0;
    
    USBFS_Start(device, USBFS_DWR_VDDD_OPERATION);
    
    xTaskCreate(&USBConfigService, "", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
}

void usb_register_interface(uint8_t interface, usb_cfg_change_cb cfg_change_cb)
{
    if (n_interfaces < max_interfaces)
    {
        // Append the interface to the list.
        interfaces[n_interfaces++] = (struct IFaceCfgDef){interface, USB_ALT_INVALID, cfg_change_cb};
    }
}

// Fun fun usb stuff.
void USBConfigService(void *arg)
{
    (void)arg;

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
            for (int i = 0; i < n_interfaces; i++)
            {
                IFaceCfg iface = &interfaces[i];
                uint8_t new_setting = USBFS_GetInterfaceSetting(iface->id);
                // Configuration changed, call the callback.
                if (iface->settings != new_setting)
                {
                    iface->settings = new_setting;
                    // Call the callback.
                    iface->callback();
                }
            }
        }
        vTaskDelayUntil(&xLastWakeTime, RefreshDelay);
    }
}
