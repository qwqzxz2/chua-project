/**
 * @file    app_config.h
 * @brief   项目全局配置 (用户可修改)
 */
#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/*======================= WiFi 配置 =======================*/
#define ESP8266_SSID            "YourWiFi"           /* WiFi 名称 */
#define ESP8266_PASS            "YourPassword"        /* WiFi 密码 */

/*======================= MQTT 配置 =======================*/
#define MQTT_BROKER             "broker.emqx.io"     /* MQTT Broker 地址 */
#define MQTT_PORT               1883                  /* MQTT 端口 */
#define MQTT_CLIENT_ID          "sems_device_01"      /* 设备 ID */
#define MQTT_USER               ""                    /* MQTT 用户名 */
#define MQTT_PASS               ""                    /* MQTT 密码 */
#define MQTT_TOPIC_DATA         "sems/data"           /* 数据上报 Topic */
#define MQTT_TOPIC_ALARM        "sems/alarm"          /* 告警 Topic */
#define MQTT_TOPIC_CMD          "sems/cmd"            /* 命令 Topic */
#define MQTT_KEEPALIVE          60                    /* Keep Alive 周期 (s) */

/*======================= 传感器校准 =======================*/
#define MQ135_R0_CALIBRATED     60.0f                 /* 洁净空气中 R0 值 */
#define TEMP_OFFSET             0.0f                  /* 温度校准偏移 */
#define HUMID_OFFSET            0.0f                  /* 湿度校准偏移 */

/*======================= 系统配置 =======================*/
#define SERIAL_BAUDRATE         115200                /* 调试串口波特率 */
#define FIRMWARE_VERSION        "SEMS_v1.0.0"         /* 固件版本号 */
#define DEVICE_LOCATION         "Office"              /* 设备位置标记 */

/*======================= 低功耗配置 =======================*/
#if LOW_POWER_ENABLE
#define SENSOR_INTERVAL_MS      10000                 /* 低功耗模式: 10s */
#define WIFI_DISABLE_ON_SLEEP   1                     /* 休眠时关闭 WiFi */
#else
#define SENSOR_INTERVAL_MS      1000                  /* 普通模式: 1s */
#endif

#endif
