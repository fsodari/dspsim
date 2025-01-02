#include "FreeRTOS.h"
#include "task.h"

#include "project_config.h"
// #include "project.h"
#include "CyLib.h"
#include "LEDCtl.h"

#include "dspsim/psoc/usb.h"
#include "dspsim/psoc/avril.h"
#include "dspsim/psoc/avril_msg.h"
#include "dspsim/psoc/vmmi.h"
#include "dspsim/psoc/vmmi_meta.h"
#include "dspsim/psoc/sram.h"
#include "dspsim/psoc/mmi_iter.h"
#include "dspsim/psoc/cobs.h"
#include "booter.h"
#include "dspsim/psoc/cdict.h"
#include "dspsim/psoc/cdict_mmi.h"

// Blink led task.
void start_blinky();

// Globals need to be defined somewhere.
USBSerialTx usb_serial_tx;
USBSerialRx usb_serial_rx;

/*
    This function will be called when the scheduler starts.
    Create all tasks here. You can use FreeRToS features since the scheduler will have started already.
*/
void vApplicationDaemonTaskStartupHook(void)
{
    // All interfaces to be used in the vmmi. The map will probably be a generated code file eventually?
    // Sram interfaces for demonstration.
    DictMMI dict = dict_mmi_create(8, 256, dfloat);
    Sram sram0 = sram_create(1024, dint32);
    {
        MIter sbegin, send, sit;
        int32_t i = 0;
        for_miter((MMI)sram0, sbegin, send, sit)
        {
            iset(sit, &i);
            i--;
        }
        endfor_miter(sbegin, send, sit);
    }
    Sram sram1 = sram_create(1024, dfloat);

    // Virtual MMI Interface
    VMMI vmmi = vmmi_create(VMMI_N_INTERFACES);
    // Metadata interface for inspecting the interfaces in the vmmi.
    VMMIMeta vmeta = vmmi_meta_create(vmmi, VMMI_META_RESERVE_SIZE);

    vmmi_register(vmmi, (MMI)vmeta, "vmeta"); // Metadata can be instantiated as part of the virtual interface.
    vmmi_register(vmmi, (MMI)dict, "dict");
    vmmi_register(vmmi, (MMI)sram0, "sram0");
    vmmi_register(vmmi, (MMI)sram1, "sram1");

    // Bootloader
    Booter booter = booter_create(BOOTLOAD_PASSWORD);

    // Avril interface. USB -> Cobs Avril, Avril -> Cobs -> USB
    Avril av = avril_start(AVRIL_N_MODES, AVRIL_MAX_MSG_SIZE, AVRIL_PRIORITY);

    // Add avril modes.
    avril_add_mode(av, AvrilVmmi, (MMI)vmmi);
    avril_add_mode(av, AvrilBootload, (MMI)booter);
    avril_add_mode(av, AvrilVMeta, (MMI)vmeta); // Metadata can also be accessed with a different mode.

    // Start usb
    USBCore usb_core = usb_start(0, USB_N_INTERFACES);

    // Start usb serial.
    USBSerial usb_serial = usb_serial_start(
        usb_core,
        USB_SERIAL_CTL_IFACE, USB_SERIAL_CTL_EP,
        USB_SERIAL_DATA_IFACE, USB_SERIAL_TX_EP, USB_SERIAL_RX_EP);

    // Create tx/rx modules from the serial interface.
    usb_serial_tx = usb_serial_tx_start(usb_serial, USB_SERIAL_TX_BUF_SIZE, USB_SERIAL_TX_PRIORITY);
    usb_serial_rx = usb_serial_rx_start(usb_serial, USB_SERIAL_RX_BUF_SIZE, USB_SERIAL_RX_PRIORITY);

    // Start the cobs encoder/decoders. Connect between the avril message buffer and serial stream buffer.
    Cobs encoder = cobs_encode_start(avril_tx_msg_buf(av), usb_serial_tx_buf(usb_serial_tx), USB_SERIAL_TX_BUF_SIZE, COBS_ENCODE_PRIORITY);
    Cobs decoder = cobs_decode_start(avril_rx_msg_buf(av), usb_serial_rx_buf(usb_serial_rx), USB_SERIAL_RX_BUF_SIZE, COBS_DECODE_PRIORITY);

    start_blinky();
    (void)encoder;
    (void)decoder;
    (void)booter;
}

static void HardwareSetup(void)
{
    /* Port layer functions that need to be copied into the vector table. */
    extern void xPortPendSVHandler(void);
    extern void xPortSysTickHandler(void);
    extern void vPortSVCHandler(void);
    extern cyisraddress CyRamVectors[];

    /* Install the OS Interrupt Handlers. */
    CyRamVectors[11] = (cyisraddress)vPortSVCHandler;
    CyRamVectors[14] = (cyisraddress)xPortPendSVHandler;
    CyRamVectors[15] = (cyisraddress)xPortSysTickHandler;
}

//
int main(void)
{
    CyGlobalIntEnable;
    HardwareSetup();
    vTaskStartScheduler();

    for (ever)
    {
    }
}

/*---------------------------------------------------------------------------*/
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pxTask;
    (void)pcTaskName;
    /* The stack space has been exceeded for a task, considering allocating more. */
    taskDISABLE_INTERRUPTS();
    for (ever)
    {
    }
}
/*---------------------------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* The heap space has been exceeded. */
    taskDISABLE_INTERRUPTS();
    for (ever)
    {
    }
}

void Blinky(void *arg)
{
    (void)arg;

    for (ever)
    {
        LEDCtl_Write(0x1);
        vTaskDelay(pdMS_TO_TICKS(500));
        LEDCtl_Write(0x0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
void start_blinky()
{
    xTaskCreate(&Blinky, "", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
