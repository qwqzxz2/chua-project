#include "stm32f4xx_it.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

void NMI_Handler(void) {}
void HardFault_Handler(void) { while(1); }
void MemManage_Handler(void) { while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void) { while(1); }
void SVC_Handler(void) { __asm("tst lr, #4\n ite eq\n movseq r0, #0\n movsne r0, #1\n b vPortSVCHandler"); }
void DebugMon_Handler(void) {}
void PendSV_Handler(void) { __asm("b vPortPendSVHandler"); }
void SysTick_Handler(void) {
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

void DMA2_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

void USART2_IRQHandler(void)
{
    extern ESP8266_Handle_t esp8266_h;
    uint8_t byte;
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        byte = (uint8_t)(huart2.Instance->DR & 0xFF);
        if (esp8266_h.rx_index < ESP8266_RX_BUF_SIZE) {
            esp8266_h.rx_buf[esp8266_h.rx_index++] = byte;
        }
    }
    HAL_UART_IRQHandler(&huart2);
}
