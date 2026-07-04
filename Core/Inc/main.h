/* main.h - 主程序头文件 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*======================= 系统配置宏 =======================*/

/* 版本信息 */
#define FIRMWARE_VERSION_MAJOR       1
#define FIRMWARE_VERSION_MINOR       0
#define FIRMWARE_VERSION_PATCH       0

/* 传感器采样间隔 (ms) */
#define SENSOR_INTERVAL_TEMP_HUMID   2000   /* DHT22 */
#define SENSOR_INTERVAL_PRESSURE     5000   /* BMP280 */
#define SENSOR_INTERVAL_AIR_QUALITY  1000   /* MQ-135 */
#define SENSOR_INTERVAL_LIGHT        1000   /* LDR */

/* 显示刷新间隔 (ms) */
#define DISPLAY_REFRESH_INTERVAL     500

/* WiFi 通信间隔 (ms) */
#define WIFI_UPLOAD_INTERVAL         30000  /* 30s 上报一次 */

/* SD 卡存储间隔 (ms) */
#define STORAGE_LOG_INTERVAL         300000 /* 5min 记录一次 */

/* 告警阈值默认值 */
#define ALARM_TEMP_HIGH_DEFAULT      40.0f
#define ALARM_TEMP_LOW_DEFAULT       -10.0f
#define ALARM_HUMID_HIGH_DEFAULT     80.0f
#define ALARM_HUMID_LOW_DEFAULT      20.0f
#define ALARM_AQI_HIGH_DEFAULT       200.0f
#define ALARM_PRESSURE_DELTA         20.0f  /* 气压剧烈变化阈值 */

/* 低功耗模式 */
#define LOW_POWER_ENABLE             0      /* 1=启用低功耗, 0=普通模式 */

/*======================= 数据类型定义 =======================*/

/* 传感器数据结构体 */
typedef struct {
    float       temperature;        /* 温度 (℃) */
    float       humidity;           /* 湿度 (%RH) */
    float       pressure;           /* 气压 (hPa) */
    float       air_quality_ppm;    /* 空气质量 (ppm CO2 等效) */
    float       light_lux;          /* 光照强度 (lux) */
    uint32_t    timestamp;          /* 采集时间戳 (ms) */
    uint8_t     valid_flags;        /* 各传感器有效标志位 */
} SensorData_t;

/* 告警事件结构体 */
typedef struct {
    uint8_t     type;               /* 告警类型 */
    uint8_t     severity;           /* 严重等级 0-3 */
    float       current_value;      /* 当前值 */
    float       threshold;          /* 触发阈值 */
    uint32_t    timestamp;          /* 触发时间 */
} AlarmEvent_t;

/* 告警类型枚举 */
typedef enum {
    ALARM_TYPE_NONE          = 0,
    ALARM_TYPE_TEMP_HIGH     = 1,
    ALARM_TYPE_TEMP_LOW      = 2,
    ALARM_TYPE_HUMID_HIGH    = 3,
    ALARM_TYPE_HUMID_LOW     = 4,
    ALARM_TYPE_AQI_HIGH      = 5,
    ALARM_TYPE_PRESSURE_DROP = 6,
    ALARM_TYPE_SENSOR_FAIL   = 7,
} AlarmType_t;

/* 告警等级枚举 */
typedef enum {
    ALARM_LEVEL_INFO    = 0,  /* 仅记录 */
    ALARM_LEVEL_WARNING = 1,  /* OLED 提示 */
    ALARM_LEVEL_CRITICAL= 2,  /* 蜂鸣器 + 提示 */
    ALARM_LEVEL_EMERGENCY=3,  /* 蜂鸣器 + LED + 远程推送 */
} AlarmLevel_t;

/* 传感器有效标志位 */
#define SENSOR_FLAG_TEMP        (1 << 0)
#define SENSOR_FLAG_HUMID       (1 << 1)
#define SENSOR_FLAG_PRESSURE    (1 << 2)
#define SENSOR_FLAG_AIR_QUALITY (1 << 3)
#define SENSOR_FLAG_LIGHT       (1 << 4)

/* MQTT JSON 数据缓冲区 */
#define MQTT_JSON_BUF_SIZE      256

/*======================= 外部全局变量 =======================*/

extern SensorData_t    g_sensor_data;
extern volatile uint32_t g_sys_tick_ms;
extern volatile uint8_t  g_wifi_connected;

/*======================= 函数声明 =======================*/

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_I2C1_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_SPI1_Init(void);
void MX_ADC1_Init(void);
void MX_TIM3_Init(void);
void MX_TIM4_Init(void);
void MX_DMA_Init(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
