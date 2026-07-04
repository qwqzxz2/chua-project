/**
 * @file    bmp280.c
 * @brief   BMP280 I2C 驱动实现
 *          使用 Bosch 官方补偿算法的轻量化实现
 */
#include "sensors/bmp280.h"

/* 读取寄存器 */
static int8_t i2c_read_reg(BMP280_Handle_t *dev, uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (HAL_I2C_Master_Transmit(dev->i2c, dev->addr, &reg, 1, 100) != HAL_OK)
        return BMP280_ERR_I2C;
    if (HAL_I2C_Master_Receive(dev->i2c, dev->addr, buf, len, 100) != HAL_OK)
        return BMP280_ERR_I2C;
    return BMP280_OK;
}

/* 写入寄存器 */
static int8_t i2c_write_reg(BMP280_Handle_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    if (HAL_I2C_Master_Transmit(dev->i2c, dev->addr, data, 2, 100) != HAL_OK)
        return BMP280_ERR_I2C;
    return BMP280_OK;
}

/**
 * @brief  读取校准参数
 */
static void read_calibration(BMP280_Handle_t *dev)
{
    uint8_t buf[24];
    if (i2c_read_reg(dev, BMP280_REG_CALIB, buf, 24) != BMP280_OK)
        return;

    dev->dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    dev->dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    dev->dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);
    dev->dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    dev->dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    dev->dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    dev->dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    dev->dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    dev->dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    dev->dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    dev->dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    dev->dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);
}

/**
 * @brief  初始化 BMP280
 *         配置: 正常模式, 温度 ×2, 气压 ×16, 滤波器 ×16
 */
int BMP280_Init(BMP280_Handle_t *dev, I2C_HandleTypeDef *i2c, uint16_t addr)
{
    dev->i2c = i2c;
    dev->addr = addr;
    dev->initialized = 0;

    /* 检查芯片 ID */
    uint8_t chip_id;
    if (i2c_read_reg(dev, BMP280_REG_ID, &chip_id, 1) != BMP280_OK)
        return BMP280_ERR_I2C;
    if (chip_id != BMP280_CHIP_ID)
        return BMP280_ERR_ID;

    /* 软复位 */
    i2c_write_reg(dev, BMP280_REG_RESET, 0xB6);
    HAL_Delay(10);

    /* 读取校准参数 */
    read_calibration(dev);

    /* 配置: 温度过采样 ×2, 气压过采样 ×16, 正常模式 */
    i2c_write_reg(dev, BMP280_REG_CTRL_MEAS, 
                  (BMP280_OSRS_TEMP_X2 << 5) | 
                  (BMP280_OSRS_X16 << 2) | 
                  BMP280_MODE_NORMAL);

    /* 配置: 滤波器 ×16, SPI 禁用 */
    i2c_write_reg(dev, BMP280_REG_CONFIG,
                  (BMP280_FILTER_16 << 2));

    HAL_Delay(100);  /* 等待第一次测量完成 */
    dev->initialized = 1;

    return BMP280_OK;
}

/**
 * @brief  读取原始温度值
 */
static int32_t read_raw_temp(BMP280_Handle_t *dev)
{
    uint8_t buf[3];
    if (i2c_read_reg(dev, BMP280_REG_TEMP_MSB, buf, 3) != BMP280_OK)
        return 0;
    return (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
}

/**
 * @brief  读取原始气压值
 */
static int32_t read_raw_pressure(BMP280_Handle_t *dev)
{
    uint8_t buf[3];
    if (i2c_read_reg(dev, BMP280_REG_PRESS_MSB, buf, 3) != BMP280_OK)
        return 0;
    return (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
}

/**
 * @brief  温度补偿计算 (Bosch 公式)
 */
static float compensate_temperature(BMP280_Handle_t *dev, int32_t adc_T)
{
    int32_t var1, var2, T;

    var1 = ((((adc_T >> 3) - ((int32_t)dev->dig_T1 << 1))) * ((int32_t)dev->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dev->dig_T1)) * ((adc_T >> 4) - ((int32_t)dev->dig_T1))) >> 12) *
            ((int32_t)dev->dig_T3)) >> 14;

    dev->t_fine = var1 + var2;
    T = (dev->t_fine * 5 + 128) >> 8;
    return T / 100.0f;
}

/**
 * @brief  气压补偿计算 (Bosch 公式)
 */
static float compensate_pressure(BMP280_Handle_t *dev, int32_t adc_P)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)dev->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dev->dig_P6;
    var2 = var2 + ((var1 * (int64_t)dev->dig_P5) << 17);
    var2 = var2 + (((int64_t)dev->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dev->dig_P3) >> 8) +
           ((var1 * (int64_t)dev->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->dig_P1) >> 33;

    if (var1 == 0) return 0;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dev->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dev->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dev->dig_P7) << 4);
    return (p / 256.0f) / 100.0f;
}

int BMP280_ReadTemperature(BMP280_Handle_t *dev, float *temp)
{
    if (!dev->initialized) return BMP280_ERR_NOT_INIT;
    int32_t raw = read_raw_temp(dev);
    *temp = compensate_temperature(dev, raw);
    return BMP280_OK;
}

int BMP280_ReadPressure(BMP280_Handle_t *dev, float *press)
{
    if (!dev->initialized) return BMP280_ERR_NOT_INIT;
    int32_t raw_p = read_raw_pressure(dev);
    int32_t raw_t = read_raw_temp(dev);
    compensate_temperature(dev, raw_t);  /* 更新 t_fine */
    *press = compensate_pressure(dev, raw_p);
    return BMP280_OK;
}

int BMP280_ReadAll(BMP280_Handle_t *dev, float *temp, float *press)
{
    if (!dev->initialized) return BMP280_ERR_NOT_INIT;
    int32_t raw_t = read_raw_temp(dev);
    int32_t raw_p = read_raw_pressure(dev);
    *temp = compensate_temperature(dev, raw_t);
    *press = compensate_pressure(dev, raw_p);
    return BMP280_OK;
}
