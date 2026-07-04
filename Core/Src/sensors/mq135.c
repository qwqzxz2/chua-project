/**
 * @file    mq135.c
 * @brief   MQ-135 驱动实现
 *          基于 ADC 采样 + 指数曲线拟合
 *          CO2 浓度计算公式: ppm = 116.6 * (Rs/R0)^-2.769
 *          参考: MQ-135 datasheet sensitivity curve
 */
#include "sensors/mq135.h"
#include <math.h>

static ADC_HandleTypeDef *mq135_adc = NULL;
static uint32_t           mq135_channel = 0;
static uint8_t            mq135_initialized = 0;

/* 滑动平均滤波器 */
#define FILTER_SIZE 5
static uint32_t filter_buf[FILTER_SIZE];
static uint8_t  filter_idx = 0;

int MQ135_Init(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    mq135_adc = hadc;
    mq135_channel = channel;
    mq135_initialized = 1;

    /* 清空滤波缓冲区 */
    for (int i = 0; i < FILTER_SIZE; i++) {
        filter_buf[i] = 0;
    }
    filter_idx = 0;

    /* 启动 ADC 连续转换 */
    HAL_ADC_Start(mq135_adc);

    return MQ135_OK;
}

int MQ135_ReadRaw(uint32_t channel, uint32_t *value)
{
    if (!mq135_initialized) return MQ135_ERR_NOT_INIT;

    /* 等待 ADC 转换完成 */
    if (HAL_ADC_PollForConversion(mq135_adc, 100) != HAL_OK) {
        return MQ135_ERR_ADC;
    }

    uint32_t adc_val = HAL_ADC_GetValue(mq135_adc);

    /* 滑动平均滤波 */
    filter_buf[filter_idx] = adc_val;
    filter_idx = (filter_idx + 1) % FILTER_SIZE;

    uint32_t sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += filter_buf[i];
    }
    *value = sum / FILTER_SIZE;

    return MQ135_OK;
}

int MQ135_ReadPPM(float *ppm)
{
    if (!mq135_initialized) return MQ135_ERR_NOT_INIT;

    uint32_t adc_val;
    if (MQ135_ReadRaw(mq135_channel, &adc_val) != MQ135_OK) {
        return MQ135_ERR_ADC;
    }

    /* ADC 值转换为电压 */
    float voltage = (float)adc_val / 4095.0f * MQ135_ADC_REF;

    /* 计算传感器电阻 Rs
     * 分压电路: Vout = Vcc * RL / (Rs + RL)
     * Rs = (Vcc / Vout - 1) * RL
     */
    if (voltage <= 0.001f) voltage = 0.001f;
    float rs = (MQ135_ADC_REF / voltage - 1.0f) * MQ135_RL;

    /* 计算 Rs/R0 比值 */
    float ratio = rs / MQ135_R0_CLEAN;

    /* 指数曲线拟合 (CO2 曲线, 来自 datasheet)
     * ppm = A * (Rs/R0)^B
     * A=116.6, B=-2.769 (对数坐标拟合)
     */
    *ppm = 116.6f * powf(ratio, -2.769f);

    /* 限幅 */
    if (*ppm < 10.0f) *ppm = 10.0f;
    if (*ppm > 2000.0f) *ppm = 2000.0f;

    return MQ135_OK;
}
