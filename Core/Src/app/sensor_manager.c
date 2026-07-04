/**
 * @file    sensor_manager.c
 * @brief   传感器管理器
 *          负责传感器的初始化、轮询调度、错误恢复
 */
#include "app/sensor_manager.h"
#include "sensors/dht22.h"
#include "sensors/bmp280.h"
#include "sensors/mq135.h"

static SensorHealth_t sensor_health = {0};
static uint32_t last_recovery_time = 0;

int SensorManager_Init(void)
{
    sensor_health.dht22_state  = SENSOR_STATE_INIT;
    sensor_health.bmp280_state = SENSOR_STATE_INIT;
    sensor_health.mq135_state  = SENSOR_STATE_INIT;
    sensor_health.total_reads  = 0;

    /* DHT22 初始化已在 main.c 中完成 */
    sensor_health.dht22_state = SENSOR_STATE_ACTIVE;

    return 0;
}

void SensorManager_Process(void)
{
    sensor_health.total_reads++;
}

void SensorManager_GetHealth(SensorHealth_t *health)
{
    *health = sensor_health;
}

int SensorManager_CalibrateMQ135(void)
{
    /* 在洁净空气中校准 MQ-135 基线 */
    uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        uint32_t val;
        if (MQ135_ReadRaw(ADC_CHANNEL_0, &val) == MQ135_OK) {
            sum += val;
        }
        HAL_Delay(10);
    }
    return (int)(sum / 100);
}
