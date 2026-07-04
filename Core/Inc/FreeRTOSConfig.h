/* FreeRTOSConfig.h - FreeRTOS 内核配置 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/*-----------------------------------------------------------
 * 处理器相关配置
 *-----------------------------------------------------------*/
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      168000000UL
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    (5)
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)
#define configTOTAL_HEAP_SIZE                   ((size_t)(40 * 1024))    /* 40KB 堆 */
#define configMAX_TASK_NAME_LEN                 (16)
#define configUSE_TRACE_FACILITY                1
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               8
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_COUNTING_SEMAPHORES           1
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

/* 可选的 API 功能 */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0

/* 软件定时器 */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (2)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

/* 中断优先级配置 */
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191 /* 优先级 5-15 可被 FreeRTOS 管理 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Cortex-M4 MPU 配置 (禁用) */
#define configUSE_MPU                           0
#define configENABLE_MPU                        0
#define configENABLE_FPU                        1
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          1

/* 断言 */
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/* 钩子函数原型 */
extern void vApplicationMallocFailedHook(void);
extern void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
extern void vApplicationIdleHook(void);

#endif /* FREERTOS_CONFIG_H */
