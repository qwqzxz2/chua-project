/**
 * @file    alarm_manager.c
 * @brief   智能告警管理器
 *          实现阈值检测、去抖滤波、冷却时间、多级告警
 */
#include "app/alarm_manager.h"
#include "app/app_common.h"
#include <math.h>

static AlarmThresholds_t thresholds = {
    .temp_high   = ALARM_TEMP_HIGH_DEFAULT,
    .temp_low    = ALARM_TEMP_LOW_DEFAULT,
    .humid_high  = ALARM_HUMID_HIGH_DEFAULT,
    .humid_low   = ALARM_HUMID_LOW_DEFAULT,
    .aqi_high    = ALARM_AQI_HIGH_DEFAULT,
    .press_delta = ALARM_PRESSURE_DELTA,
};

/* 去抖计数器 */
static uint8_t debounce_temp_high = 0;
static uint8_t debounce_temp_low = 0;
static uint8_t debounce_humid_high = 0;
static uint8_t debounce_humid_low = 0;
static uint8_t debounce_aqi_high = 0;

/* 上次告警时间戳 (分别跟踪不同类型) */
#define NUM_ALARM_TIMERS 8
static uint32_t last_alarm_time[NUM_ALARM_TIMERS] = {0};
static float last_pressure = 1013.25f;

int AlarmManager_Init(void)
{
    for (int i = 0; i < NUM_ALARM_TIMERS; i++) {
        last_alarm_time[i] = 0;
    }
    debounce_temp_high = debounce_temp_low = 0;
    debounce_humid_high = debounce_humid_low = 0;
    debounce_aqi_high = 0;
    last_pressure = 1013.25f;
    return 0;
}

void AlarmManager_SetThresholds(const AlarmThresholds_t *t)
{
    thresholds = *t;
}

void AlarmManager_Check(const SensorData_t *data)
{
    uint32_t now = HAL_GetTick();
    AlarmEvent_t alarm = {0};

    /* 温度过高检测 (带去抖) */
    if (data->valid_flags & SENSOR_FLAG_TEMP) {
        if (data->temperature > thresholds.temp_high) {
            if (++debounce_temp_high >= ALARM_DEBOUNCE_COUNT) {
                debounce_temp_high = 0;
                if (now - last_alarm_time[ALARM_TYPE_TEMP_HIGH] > ALARM_COOLDOWN_MS) {
                    alarm.type = ALARM_TYPE_TEMP_HIGH;
                    alarm.severity = (data->temperature > thresholds.temp_high + 10.0f)
                                   ? ALARM_LEVEL_CRITICAL : ALARM_LEVEL_WARNING;
                    alarm.current_value = data->temperature;
                    alarm.threshold = thresholds.temp_high;
                    alarm.timestamp = now;
                    xQueueSend(xQueueAlarmEvent, &alarm, 0);
                    last_alarm_time[ALARM_TYPE_TEMP_HIGH] = now;
                }
            }
        } else {
            debounce_temp_high = 0;
        }
    }

    /* 湿度过高检测 */
    if (data->valid_flags & SENSOR_FLAG_HUMID) {
        if (data->humidity > thresholds.humid_high) {
            if (++debounce_humid_high >= ALARM_DEBOUNCE_COUNT) {
                debounce_humid_high = 0;
                if (now - last_alarm_time[ALARM_TYPE_HUMID_HIGH] > ALARM_COOLDOWN_MS) {
                    alarm.type = ALARM_TYPE_HUMID_HIGH;
                    alarm.severity = ALARM_LEVEL_WARNING;
                    alarm.current_value = data->humidity;
                    alarm.threshold = thresholds.humid_high;
                    alarm.timestamp = now;
                    xQueueSend(xQueueAlarmEvent, &alarm, 0);
                    last_alarm_time[ALARM_TYPE_HUMID_HIGH] = now;
                }
            }
        } else {
            debounce_humid_high = 0;
        }
    }

    /* 空气质量过高检测 */
    if (data->valid_flags & SENSOR_FLAG_AIR_QUALITY) {
        if (data->air_quality_ppm > thresholds.aqi_high) {
            if (++debounce_aqi_high >= ALARM_DEBOUNCE_COUNT) {
                debounce_aqi_high = 0;
                if (now - last_alarm_time[ALARM_TYPE_AQI_HIGH] > ALARM_COOLDOWN_MS) {
                    alarm.type = ALARM_TYPE_AQI_HIGH;
                    alarm.severity = ALARM_LEVEL_CRITICAL;
                    alarm.current_value = data->air_quality_ppm;
                    alarm.threshold = thresholds.aqi_high;
                    alarm.timestamp = now;
                    xQueueSend(xQueueAlarmEvent, &alarm, 0);
                    last_alarm_time[ALARM_TYPE_AQI_HIGH] = now;
                }
            }
        } else {
            debounce_aqi_high = 0;
        }
    }

    /* 气压剧变检测 (恶劣天气预警) */
    if (data->valid_flags & SENSOR_FLAG_PRESSURE) {
        float delta = data->pressure - last_pressure;
        if (fabsf(delta) > thresholds.press_delta) {
            if (now - last_alarm_time[ALARM_TYPE_PRESSURE_DROP] > ALARM_COOLDOWN_MS) {
                alarm.type = ALARM_TYPE_PRESSURE_DROP;
                alarm.severity = ALARM_LEVEL_WARNING;
                alarm.current_value = delta;
                alarm.threshold = thresholds.press_delta;
                alarm.timestamp = now;
                xQueueSend(xQueueAlarmEvent, &alarm, 0);
                last_alarm_time[ALARM_TYPE_PRESSURE_DROP] = now;
            }
        }
        last_pressure = data->pressure;
    }
}

void AlarmManager_GetThresholds(AlarmThresholds_t *t)
{
    *t = thresholds;
}

void AlarmManager_Silence(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
}
