/**
 * @file    mqtt.h
 * @brief   Lightweight MQTT client over ESP8266 AT driver
 */

#ifndef __MQTT_H__
#define __MQTT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "esp8266.h"
#include <stdint.h>
#include <string.h>

/* Exported defines ----------------------------------------------------------*/

/** @defgroup MQTT_Status Status codes
 *  @{
 */
#define MQTT_OK            (0)
#define MQTT_ERR_CONNECT   (-1)
#define MQTT_ERR_PUBLISH   (-2)
#define MQTT_ERR_SUBSCRIBE (-3)
/** @} */

/** @defgroup MQTT_Protocol Constants
 *  @{
 */
#define MQTT_DEFAULT_PORT       (1883)
#define MQTT_KEEPALIVE_SEC      (60)
#define MQTT_PROTOCOL_LEVEL     (4)     /**< MQTT 3.1.1 */
#define MQTT_CONNECT_FLAG_CLEAN (0x02)

/* Default MQTT configuration — users may override in app_config.h */
#ifndef MQTT_BROKER
#define MQTT_BROKER      "broker.emqx.io"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT        1883
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID   "sems_device_01"
#endif

#ifndef MQTT_USER
#define MQTT_USER        ""
#endif

#ifndef MQTT_PASS
#define MQTT_PASS        ""
#endif

#ifndef MQTT_TOPIC_DATA
#define MQTT_TOPIC_DATA   "sems/data"
#endif

#ifndef ESP8266_SSID
#define ESP8266_SSID     "YourWiFi"
#endif

#ifndef ESP8266_PASS
#define ESP8266_PASS     "YourPassword"
#endif
/** @} */

/** @defgroup MQTT_Packet_Type MQTT packet type nibbles
 *  @{
 */
#define MQTT_PKT_CONNECT     (0x10)
#define MQTT_PKT_CONNACK     (0x20)
#define MQTT_PKT_PUBLISH     (0x30)
#define MQTT_PKT_PUBACK      (0x40)
#define MQTT_PKT_SUBSCRIBE   (0x82)
#define MQTT_PKT_SUBACK      (0x90)
#define MQTT_PKT_PINGREQ     (0xC0)
#define MQTT_PKT_PINGRESP    (0xD0)
#define MQTT_PKT_DISCONNECT  (0xE0)
/** @} */

/* Exported types ------------------------------------------------------------*/

/**
 * @brief  MQTT QoS levels
 */
typedef enum
{
    MQTT_QOS0 = 0,
    MQTT_QOS1 = 1,
    MQTT_QOS2 = 2
} MQTT_QoS_t;

/**
 * @brief  MQTT client state
 */
typedef enum
{
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_DISCONNECTING
} MQTT_State_t;

/**
 * @brief  MQTT client handle structure
 */
typedef struct
{
    ESP8266_Handle_t *esp;          /**< Pointer to ESP8266 handle */
    MQTT_State_t      state;        /**< Current MQTT state */
    uint16_t          last_packet_id; /**< Last used packet identifier */
    uint32_t          last_keepalive_ms; /**< Last keepalive timestamp (ms) */
    uint8_t           initialized;  /**< Non-zero if initialized */
} MQTT_Handle_t;

/* Exported function prototypes ----------------------------------------------*/

/**
 * @brief  Initialize the MQTT client handle
 * @param  handle  Pointer to MQTT handle
 * @param  esp_handle Pointer to initialized ESP8266 handle
 * @retval MQTT_OK on success
 */
int8_t MQTT_Init(MQTT_Handle_t *handle, ESP8266_Handle_t *esp_handle);

/**
 * @brief  Establish MQTT connection to broker
 * @param  broker   Broker hostname or IP (null-terminated)
 * @param  port     Broker TCP port
 * @param  client_id Client identifier (null-terminated)
 * @param  user     Username (may be empty string)
 * @param  pass     Password (may be empty string)
 * @retval MQTT_OK on success, else error code
 */
int8_t MQTT_Connect(MQTT_Handle_t *handle,
                    const char *broker,
                    uint16_t port,
                    const char *client_id,
                    const char *user,
                    const char *pass);

/**
 * @brief  Publish a message to a topic
 * @param  handle  Pointer to MQTT handle
 * @param  topic   Topic string (null-terminated)
 * @param  payload Payload data (may contain null bytes)
 * @param  qos     QoS level (0, 1, or 2)
 * @retval MQTT_OK on success, else error code
 */
int8_t MQTT_Publish(MQTT_Handle_t *handle,
                    const char *topic,
                    const uint8_t *payload,
                    uint8_t qos);

/**
 * @brief  Subscribe to a topic filter
 * @param  handle Pointer to MQTT handle
 * @param  topic  Topic filter string (null-terminated)
 * @param  qos    Maximum QoS level
 * @retval MQTT_OK on success, else error code
 */
int8_t MQTT_Subscribe(MQTT_Handle_t *handle,
                      const char *topic,
                      uint8_t qos);

/**
 * @brief  Send a PINGREQ keepalive (call periodically)
 * @param  handle Pointer to MQTT handle
 * @retval MQTT_OK on success, else error code
 */
int8_t MQTT_KeepAlive(MQTT_Handle_t *handle);

/**
 * @brief  Disconnect from the MQTT broker gracefully
 * @param  handle Pointer to MQTT handle
 * @retval MQTT_OK on success, else error code
 */
int8_t MQTT_Disconnect(MQTT_Handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_H__ */
