/**
 * @file    dht22.c
 * @brief   DHT22 单总线驱动实现 (位操作时序)
 *          协议: MCU 拉低 >18ms → 拉高 20-40μs → DHT22 响应 80μs L + 80μs H
 *          → 数据 40bit (16bit 湿度 + 16bit 温度 + 8bit CRC)
 */
#include "sensors/dht22.h"
#include <string.h>

/* 私有变量 */
static GPIO_TypeDef  *dht22_port = NULL;
static uint16_t       dht22_pin  = 0;
static uint32_t       dht22_last_read_ms = 0;

/* 微秒级延时 (DWT 循环计数) */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (168 / 2);  /* ~84 cycles/us @ 168MHz */
    while ((DWT->CYCCNT - start) < cycles);
}

/**
 * @brief  初始化 DHT22
 */
int DHT22_Init(GPIO_TypeDef *port, uint16_t pin)
{
    dht22_port = port;
    dht22_pin  = pin;

    /* 使能 DWT 周期计数器 (用于微秒延时) */
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    /* 初始状态: 总线拉高 (空闲) */
    HAL_GPIO_WritePin(dht22_port, dht22_pin, GPIO_PIN_SET);
    HAL_GPIO_Init(dht22_port, &(GPIO_InitTypeDef){
        .Pin = dht22_pin,
        .Mode = GPIO_MODE_OUTPUT_OD,
        .Pull = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_HIGH
    });

    return DHT22_OK;
}

/**
 * @brief  读取 DHT22 数据
 * @note   单总线通信时序:
 *         1. 主机拉低 18ms 启动传输
 *         2. 主机拉高 20-40μs
 *         3. DHT22 拉低 80μs 响应
 *         4. DHT22 拉高 80μs 准备数据
 *         5. 40bit 数据: 0=50μs H, 1=70μs H
 */
int DHT22_Read(float *temperature, float *humidity)
{
    if (!dht22_port) return DHT22_BUSY;

    /* 最小采样间隔 2s */
    uint32_t now = HAL_GetTick();
    if (now - dht22_last_read_ms < 2000) {
        return DHT22_BUSY;
    }

    uint8_t data[5] = {0};
    uint32_t timeout;
    uint8_t mask;

    /* ==== 启动信号: 主机拉低 > 18ms ==== */
    GPIO_InitTypeDef gpio_out = {
        .Pin = dht22_pin,
        .Mode = GPIO_MODE_OUTPUT_OD,
        .Pull = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_HIGH
    };
    HAL_GPIO_Init(dht22_port, &gpio_out);
    HAL_GPIO_WritePin(dht22_port, dht22_pin, GPIO_PIN_RESET);
    HAL_Delay(20);                          /* 拉低 20ms */
    HAL_GPIO_WritePin(dht22_port, dht22_pin, GPIO_PIN_SET);
    delay_us(30);                           /* 拉高 30μs */

    /* ==== 切换为输入，等待 DHT22 响应 ==== */
    GPIO_InitTypeDef gpio_in = {
        .Pin = dht22_pin,
        .Mode = GPIO_MODE_INPUT,
        .Pull = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_HIGH
    };
    HAL_GPIO_Init(dht22_port, &gpio_in);

    /* 等待 DHT22 拉低 (响应信号) — 超时 100μs */
    timeout = 100;
    while (HAL_GPIO_ReadPin(dht22_port, dht22_pin) && timeout--) {
        delay_us(1);
    }
    if (timeout == 0) return DHT22_TIMEOUT;

    /* 等待 DHT22 拉高 (准备数据) — 超时 100μs */
    timeout = 100;
    while (!HAL_GPIO_ReadPin(dht22_port, dht22_pin) && timeout--) {
        delay_us(1);
    }
    if (timeout == 0) return DHT22_TIMEOUT;

    /* 等待 DHT22 再次拉低 (数据开始) — 超时 100μs */
    timeout = 100;
    while (HAL_GPIO_ReadPin(dht22_port, dht22_pin) && timeout--) {
        delay_us(1);
    }
    if (timeout == 0) return DHT22_TIMEOUT;

    /* ==== 读取 40bit 数据 ==== */
    for (int byte = 0; byte < 5; byte++) {
        for (int bit = 7; bit >= 0; bit--) {
            /* 等待 DHT22 拉高 */
            timeout = 80;
            while (!HAL_GPIO_ReadPin(dht22_port, dht22_pin) && timeout--) {
                delay_us(1);
            }
            if (timeout == 0) return DHT22_TIMEOUT;

            /* 延时 30μs 后判断电平:
             * 0: 拉高 26-28μs → 0 应采样为低
             * 1: 拉高 70μs → 1 应采样为高
             */
            delay_us(30);
            if (HAL_GPIO_ReadPin(dht22_port, dht22_pin)) {
                data[byte] |= (1 << bit);   /* 位 = 1 */
            }

            /* 等待 DHT22 拉低 (位间隙) */
            timeout = 60;
            while (HAL_GPIO_ReadPin(dht22_port, dht22_pin) && timeout--) {
                delay_us(1);
            }
            if (timeout == 0) return DHT22_TIMEOUT;
        }
    }

    /* ==== CRC 校验 ==== */
    uint8_t crc = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (crc != data[4]) {
        return DHT22_CRC_ERROR;
    }

    /* ==== 数据解析 ====
     * data[0..1]: 湿度 (16bit, 10x 倍率)
     * data[2..3]: 温度 (16bit, 10x 倍率, bit15=符号位)
     */
    int16_t raw_humid = ((int16_t)data[0] << 8) | data[1];
    int16_t raw_temp  = ((int16_t)data[2] << 8) | data[3];

    *humidity    = raw_humid / 10.0f;
    *temperature = (raw_temp & 0x7FFF) / 10.0f;

    /* 负温度处理 (bit15 为符号位) */
    if (raw_temp & 0x8000) {
        *temperature = -*temperature;
    }

    dht22_last_read_ms = now;
    return DHT22_OK;
}
