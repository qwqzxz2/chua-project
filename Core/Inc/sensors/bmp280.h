/**
 * @file    bmp280.h
 * @brief   BMP280 气压传感器驱动 (I2C 接口)
 *          量程: 300-1100hPa, 精度 ±1hPa
 *          温度: -40~85°C, 精度 ±0.5°C
 */
#ifndef __BMP280_H
#define __BMP280_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* BMP280 I2C 地址 (SDO 接地) */
#define BMP280_ADDR_CSB_LOW     (0x76 << 1)
#define BMP280_ADDR_CSB_HIGH    (0x77 << 1)

/* 寄存器地址 */
#define BMP280_REG_ID           0xD0
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_TEMP_MSB     0xFA
#define BMP280_REG_CALIB        0x88

/* 芯片 ID 期望值 */
#define BMP280_CHIP_ID          0x58

/* 状态码 */
#define BMP280_OK               0
#define BMP280_ERR_I2C          1
#define BMP280_ERR_ID           2
#define BMP280_ERR_NOT_INIT     3

/* 采样模式 */
#define BMP280_MODE_SLEEP       0x00
#define BMP280_MODE_FORCED      0x01
#define BMP280_MODE_NORMAL      0x03

/* 过采样率 */
#define BMP280_OSRS_NONE        0x00
#define BMP280_OSRS_X1          0x01
#define BMP280_OSRS_X2          0x02
#define BMP280_OSRS_X4          0x03
#define BMP280_OSRS_X8          0x04
#define BMP280_OSRS_X16         0x05

/* 滤波器系数 */
#define BMP280_FILTER_OFF       0x00
#define BMP280_FILTER_2         0x01
#define BMP280_FILTER_4         0x02
#define BMP280_FILTER_8         0x03
#define BMP280_FILTER_16        0x04

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint16_t           addr;
    uint8_t            initialized;

    /* 校准参数 (从芯片读取) */
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

    /* 原始值缓存 */
    int32_t  t_fine;
} BMP280_Handle_t;

/**
 * @brief  初始化 BMP280
 * @param  dev  设备句柄
 * @param  i2c  I2C 句柄
 * @param  addr I2C 地址
 * @return BMP280_OK 表示成功
 */
int BMP280_Init(BMP280_Handle_t *dev, I2C_HandleTypeDef *i2c, uint16_t addr);

/**
 * @brief  读取温度
 * @param  dev  设备句柄
 * @param  temp 输出温度 (℃)
 * @return BMP280_OK 表示成功
 */
int BMP280_ReadTemperature(BMP280_Handle_t *dev, float *temp);

/**
 * @brief  读取气压
 * @param  dev     设备句柄
 * @param  press   输出气压 (hPa)
 * @return BMP280_OK 表示成功
 */
int BMP280_ReadPressure(BMP280_Handle_t *dev, float *press);

/**
 * @brief  同时读取温度和气压
 */
int BMP280_ReadAll(BMP280_Handle_t *dev, float *temp, float *press);

#endif
