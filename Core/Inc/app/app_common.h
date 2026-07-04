/**
 * @file    app_common.h
 * @brief   应用层公共定义
 */
#ifndef __APP_COMMON_H
#define __APP_COMMON_H

#include "main.h"

/* 全局外设句柄引用 */
extern I2C_HandleTypeDef   hi2c1;
extern UART_HandleTypeDef  huart1;
extern UART_HandleTypeDef  huart2;
extern SPI_HandleTypeDef   hspi1;
extern ADC_HandleTypeDef   hadc1;
extern TIM_HandleTypeDef   htim3;
extern TIM_HandleTypeDef   htim4;

/* 传感器句柄 */
extern BMP280_Handle_t     bmp280_h;
extern ESP8266_Handle_t    esp8266_h;

/* FreeRTOS 内核对象引用 */
extern QueueHandle_t       xQueueSensorData;
extern QueueHandle_t       xQueueAlarmEvent;

/* 引脚宏定义 (匹配硬件连接文档) */
#define DHT22_GPIO_Port         GPIOB
#define DHT22_Pin               GPIO_PIN_0
#define SD_CD_GPIO_Port         GPIOC
#define SD_CD_Pin               GPIO_PIN_0
#define ESP_RST_GPIO_Port       GPIOC
#define ESP_RST_Pin             GPIO_PIN_1
#define ESP_GPIO0_GPIO_Port     GPIOC
#define ESP_GPIO0_Pin           GPIO_PIN_2
#define SPI_CS_Pin              GPIO_PIN_4

/* RGB LED */
void RGB_LED_SetColor(uint8_t r, uint8_t g, uint8_t b);

/* ADC 通道宏 */
#define ADC_CHANNEL_0           ADC_CHANNEL_0
#define ADC_CHANNEL_1           ADC_CHANNEL_1

#endif
