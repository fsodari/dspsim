
#include "project_config.h"
// #include "project.h"
#include "CyLib.h"
#include "LEDCtl.h"

#include "dspsim/usb.h"
#include "dspsim/avril.h"
#include "dspsim/sram.h"
#include "dspsim/cobs.h"
#include "booter.h"

#include "FreeRTOS.h"
#include "task.h"

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
    // Avril interface. USB -> Cobs Avril, Avril -> Cobs -> USB
    Avril av = avril_start(AVRIL_N_MODES, AVRIL_MAX_MSG_SIZE, AVRIL_PRIORITY);
    // Add avril modes.
    avril_add_mode(av, AVRIL_MODE_STANDARD, (MMI)sram_create(1024));
    avril_add_mode(av, AVRIL_MODE_BOOTLOAD, (MMI)booter_create(BOOTLOAD_PASSWORD));

    // Start usb
    USBCore usb_core = usb_start(0, 10);
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
