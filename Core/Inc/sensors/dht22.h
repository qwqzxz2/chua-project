/**
 * @file    dht22.h
 * @brief   DHT22 温湿度传感器驱动 (单总线协议)
 *          精度: ±0.5°C, ±2%RH
 *          采样周期: 最少 2s
 */
#ifndef __DHT22_H
#define __DHT22_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* DHT22 返回状态 */
#define DHT22_OK        0
#define DHT22_TIMEOUT   1
#define DHT22_CRC_ERROR 2
#define DHT22_BUSY      3

/**
 * @brief  初始化 DHT22 GPIO
 * @param  port  GPIO 端口 (如 GPIOB)
 * @param  pin   GPIO 引脚 (如 GPIO_PIN_0)
 * @return DHT22_OK 表示成功
 */
int DHT22_Init(GPIO_TypeDef *port, uint16_t pin);

/**
 * @brief  读取温湿度
 * @param  temperature  输出温度 (℃)
 * @param  humidity     输出湿度 (%RH)
 * @return DHT22_OK 表示成功
 */
int DHT22_Read(float *temperature, float *humidity);

#endif
