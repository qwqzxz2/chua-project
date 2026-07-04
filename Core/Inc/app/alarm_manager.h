/**
 * @file    alarm_manager.h
 * @brief   智能告警管理模块
 *          多级阈值检测、去抖、防误报
 */
#ifndef __ALARM_MANAGER_H
#define __ALARM_MANAGER_H

#include "main.h"

#define ALARM_DEBOUNCE_COUNT  3   /* 连续触发 N 次才告警 */
#define ALARM_COOLDOWN_MS     300000 /* 同一类型告警冷却 (5min) */

typedef struct {
    float temp_high;
    float temp_low;
    float humid_high;
    float humid_low;
    float aqi_high;
    float press_delta;          /* 气压短时变化阈值 (hPa/h) */
} AlarmThresholds_t;

int  AlarmManager_Init(void);
void AlarmManager_SetThresholds(const AlarmThresholds_t *thresholds);
void AlarmManager_Check(const SensorData_t *data);
void AlarmManager_GetThresholds(AlarmThresholds_t *thresholds);
void AlarmManager_Silence(void);

#endif
