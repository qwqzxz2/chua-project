/**
 * @file    wifi_manager.c
 * @brief   WiFi 连接管理器
 *          状态机: 初始化 → 连接 → MQTT → 活跃 (含断线重连)
 */
#include "app/wifi_manager.h"
#include "comm/esp8266.h"
#include "comm/mqtt.h"

static WiFiStatus_t wifi_status = {0};

/* 重连间隔表 (指数退避): 1s, 2s, 4s, 8s, 16s, 32s, 60s */
static const uint32_t backoff_table[] = {1000, 2000, 4000, 8000, 16000, 32000, 60000};
#define BACKOFF_TABLE_SIZE (sizeof(backoff_table) / sizeof(backoff_table[0]))

int WiFiManager_Init(void)
{
    wifi_status.state = WIFI_STATE_INIT;
    wifi_status.reconnect_interval = 1000;
    wifi_status.failed_attempts = 0;
    wifi_status.rssi = -100;
    wifi_status.uptime_ms = 0;
    return 0;
}

void WiFiManager_Process(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();

    switch (wifi_status.state) {
        case WIFI_STATE_INIT:
            if (ESP8266_Init(&esp8266_h, &huart2,
                             ESP_RST_GPIO_Port, ESP_RST_Pin,
                             ESP_GPIO0_GPIO_Port, ESP_GPIO0_Pin) == ESP8266_OK) {
                wifi_status.state = WIFI_STATE_CONNECTING;
                printf("[WiFi] ESP8266 initialized\r\n");
            } else {
                wifi_status.state = WIFI_STATE_ERROR;
            }
            break;

        case WIFI_STATE_CONNECTING:
            if (now - last_tick < wifi_status.reconnect_interval)
                break;
            last_tick = now;

            if (ESP8266_ConnectAP(ESP8266_SSID, ESP8266_PASS) == ESP8266_OK) {
                wifi_status.state = WIFI_STATE_MQTT_CONNECT;
                wifi_status.failed_attempts = 0;
                wifi_status.reconnect_interval = 1000;
                printf("[WiFi] Connected to AP\r\n");
            } else {
                wifi_status.failed_attempts++;
                uint8_t idx = wifi_status.failed_attempts < BACKOFF_TABLE_SIZE
                            ? wifi_status.failed_attempts : (BACKOFF_TABLE_SIZE - 1);
                wifi_status.reconnect_interval = backoff_table[idx];
                printf("[WiFi] Retry %lu in %lu ms\r\n",
                       wifi_status.failed_attempts, wifi_status.reconnect_interval);
            }
            break;

        case WIFI_STATE_MQTT_CONNECT:
            if (MQTT_Connect(MQTT_BROKER, MQTT_PORT,
                             MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS) == MQTT_OK) {
                wifi_status.state = WIFI_STATE_ACTIVE;
                g_wifi_connected = 1;
                printf("[WiFi] MQTT connected\r\n");
            } else {
                wifi_status.state = WIFI_STATE_CONNECTING;
                wifi_status.reconnect_interval = 5000;
            }
            break;

        case WIFI_STATE_ACTIVE:
            wifi_status.uptime_ms += (now - last_tick);
            last_tick = now;

            /* 定期发送心跳 */
            static uint32_t last_keepalive = 0;
            if (now - last_keepalive > 30000) {
                MQTT_KeepAlive();
                last_keepalive = now;
            }
            break;

        case WIFI_STATE_ERROR:
            /* 等待 30s 后重试 */
            if (now - last_tick > 30000) {
                wifi_status.state = WIFI_STATE_INIT;
                last_tick = now;
            }
            break;

        default:
            break;
    }
}

void WiFiManager_GetStatus(WiFiStatus_t *status)
{
    *status = wifi_status;
}

int WiFiManager_ForceReconnect(void)
{
    wifi_status.state = WIFI_STATE_CONNECTING;
    wifi_status.reconnect_interval = 100;
    g_wifi_connected = 0;
    return 0;
}

void WiFiManager_SendData(const char *json_data)
{
    if (wifi_status.state != WIFI_STATE_ACTIVE) return;

    if (MQTT_Publish(MQTT_TOPIC_DATA, json_data, 0) != MQTT_OK) {
        printf("[WiFi] Publish failed, reconnecting...\r\n");
        wifi_status.state = WIFI_STATE_CONNECTING;
        g_wifi_connected = 0;
    }
}
