/**
 * @file    wifi_manager.h
 * @brief   WiFi 连接管理模块
 *          状态机驱动, 自动重连, 心跳维护
 */
#ifndef __WIFI_MANAGER_H
#define __WIFI_MANAGER_H

#include "main.h"

/* WiFi 状态 */
typedef enum {
    WIFI_STATE_DISABLED    = 0,
    WIFI_STATE_INIT        = 1,
    WIFI_STATE_CONNECTING  = 2,
    WIFI_STATE_CONNECTED   = 3,
    WIFI_STATE_MQTT_CONNECT = 4,
    WIFI_STATE_ACTIVE      = 5,
    WIFI_STATE_ERROR       = 6,
} WiFiState_t;

typedef struct {
    WiFiState_t state;
    uint32_t    reconnect_interval;   /* 当前重连间隔 (ms) */
    uint32_t    uptime_ms;            /* 已连接时长 */
    uint32_t    failed_attempts;
    int8_t      rssi;                 /* 信号强度 dBm */
    uint8_t     ip_addr[4];
} WiFiStatus_t;

int  WiFiManager_Init(void);
void WiFiManager_Process(void);
void WiFiManager_GetStatus(WiFiStatus_t *status);
int  WiFiManager_ForceReconnect(void);
void WiFiManager_SendData(const char *json_data);

#endif
