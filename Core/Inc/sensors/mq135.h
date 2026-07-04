/**
 * @file    mq135.h
 * @brief   MQ-135 空气质量传感器 + LDR 光照传感器 (ADC)
 *          MQ-135: 检测 CO2/NH3/苯/烟雾, 模拟电压输出
 *          LDR: 光敏电阻 + 10kΩ 分压, 模拟电压输出
 */
#ifndef __MQ135_H
#define __MQ135_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MQ135_OK        0
#define MQ135_ERR_ADC   1
#define MQ135_ERR_NOT_INIT 2

/* 默认参数 */
#define MQ135_RL         20.0f   /* 负载电阻 (kΩ) */
#define MQ135_R0_CLEAN   60.0f   /* 洁净空气中 R0 (kΩ) */
#define MQ135_ADC_REF    3.3f    /* ADC 参考电压 */

/**
 * @brief  初始化 MQ-135 (ADC 通道)
 * @param  hadc    ADC 句柄
 * @param  channel ADC 通道 (MQ-135)
 * @return MQ135_OK 表示成功
 */
int MQ135_Init(ADC_HandleTypeDef *hadc, uint32_t channel);

/**
 * @brief  读取 MQ-135 原始 ADC 值
 * @param  channel ADC 通道
 * @param  value   输出 ADC 原始值 (12bit)
 * @return 0 表示成功
 */
int MQ135_ReadRaw(uint32_t channel, uint32_t *value);

/**
 * @brief  读取 MQ-135 等效 CO2 浓度 (ppm)
 * @param  ppm 输出 CO2 浓度 (ppm)
 * @return MQ135_OK 表示成功
 */
int MQ135_ReadPPM(float *ppm);

#endif
