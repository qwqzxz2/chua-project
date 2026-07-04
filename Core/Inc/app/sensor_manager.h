/**
 * @file    sensor_manager.h
 * @brief   传感器管理模块
 *          协调多传感器轮询、数据融合、滤波校准
 */
#ifndef __SENSOR_MANAGER_H
#define __SENSOR_MANAGER_H

#include "main.h"

/* 传感器状态 */
typedef enum {
    SENSOR_STATE_INIT      = 0,
    SENSOR_STATE_ACTIVE    = 1,
    SENSOR_STATE_ERROR     = 2,
    SENSOR_STATE_RECOVERY  = 3,
} SensorState_t;

/* 传感器健康信息 */
typedef struct {
    SensorState_t dht22_state;
    SensorState_t bmp280_state;
    SensorState_t mq135_state;
    uint32_t      dht22_err_count;
    uint32_t      bmp280_err_count;
    uint32_t      mq135_err_count;
    uint32_t      total_reads;
    uint8_t       recovery_attempts;
} SensorHealth_t;

int  SensorManager_Init(void);
void SensorManager_Process(void);
void SensorManager_GetHealth(SensorHealth_t *health);
int  SensorManager_CalibrateMQ135(void);

#endif
