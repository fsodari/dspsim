#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <cyfitter.h>

#define configUSE_PREEMPTION 1
#define configUSE_IDLE_HOOK 0
#define configMAX_PRIORITIES (5)
#define configUSE_TICK_HOOK 0
#define configCPU_CLOCK_HZ ((unsigned long)BCLK__BUS_CLK__HZ)
#define configTICK_RATE_HZ ((TickType_t)1000)
#define configMINIMAL_STACK_SIZE ((unsigned short)128)
#define configTOTAL_HEAP_SIZE ((size_t)(58 * 1024))
#define configMAX_TASK_NAME_LEN (1)
#define configUSE_TRACE_FACILITY 0
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 0

#define configUSE_MUTEXES 1

#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 3
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE

#define configUSE_SB_COMPLETED_CALLBACK 1

#define configUSE_DAEMON_TASK_STARTUP_HOOK 1

#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_ALTERNATIVE_API 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_RECURSIVE_MUTEXES 1
#define configQUEUE_REGISTRY_SIZE 10
#define configGENERATE_RUN_TIME_STATS 0
#define configUSE_MALLOC_FAILED_HOOK 1

#define configTASK_NOTIFICATION_ARRAY_ENTRIES 4

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskCleanUpResources 0
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_eTaskGetState 1

/**
 * Configure the number of priority bits. This is normally
 * __NVIC_PRIO_BITS but PSoC Creator beta 5 contained a larger
 * value for the priority than is implemented in the hardware so
 * set it here to what the data sheet describes.
 */
#define configPRIO_BITS 3 /* 8 priority levels */

/* The lowest priority. */
#define configKERNEL_INTERRUPT_PRIORITY (7 << (8 - configPRIO_BITS))

/* Priority 5, or 160 as only the top three bits are implemented. */
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))

#endif /* FREERTOS_CONFIG_H */
