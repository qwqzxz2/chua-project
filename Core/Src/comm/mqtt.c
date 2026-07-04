/**
 * @file    mqtt.c
 * @brief   Lightweight MQTT 3.1.1 client over ESP8266 AT driver
 *
 * Builds raw MQTT packets (CONNECT, PUBLISH, PINGREQ, DISCONNECT)
 * and sends them via ESP8266_TCPSend() over the ESP8266 AT driver.
 */

/* Includes ------------------------------------------------------------------*/
#include "mqtt.h"

/* Private defines -----------------------------------------------------------*/

/** Maximum remaining length encoding bytes (MQTT spec §2.2.3) */
#define MQTT_MAX_REMLEN_BYTES 4

/** Packet identifier pool start */
#define MQTT_PACKET_ID_START  1

/* Private function prototypes ----------------------------------------------*/
static uint8_t MQTT_EncodeRemLen(uint32_t length, uint8_t *buf);
static int8_t  MQTT_SendPacket(MQTT_Handle_t *handle,
                               const uint8_t *packet,
                               uint16_t len);

/* ---------------------------------------------------------------------------*/
/*               Remaining Length Encoding (MQTT §2.2.3)                      */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Encode remaining length into variable-length byte array
 * @param  length Remaining length value (0 – 268,435,455)
 * @param  buf    Output buffer (at least 4 bytes)
 * @return Number of bytes written (1 – 4)
 */
static uint8_t MQTT_EncodeRemLen(uint32_t length, uint8_t *buf)
{
    uint8_t count = 0;

    do
    {
        uint8_t byte = length & 0x7F;
        length >>= 7;
        if (length > 0)
        {
            byte |= 0x80;
        }
        buf[count++] = byte;
    } while (length > 0 && count < MQTT_MAX_REMLEN_BYTES);

    return count;
}

/* ---------------------------------------------------------------------------*/
/*               Packet Transmit Helper                                       */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Send a raw MQTT packet via the ESP8266 TCP link
 * @param  handle Pointer to MQTT handle
 * @param  packet Raw packet bytes
 * @param  len    Packet length
 * @return MQTT_OK on success, else error
 */
static int8_t MQTT_SendPacket(MQTT_Handle_t *handle,
                              const uint8_t *packet,
                              uint16_t len)
{
    if (handle == NULL || handle->initialized == 0 ||
        handle->esp == NULL)
    {
        return MQTT_ERR_CONNECT;
    }

    if (ESP8266_TCPSend(handle->esp, packet, len) != ESP8266_OK)
    {
        return MQTT_ERR_PUBLISH;
    }

    return MQTT_OK;
}

/* ---------------------------------------------------------------------------*/
/*               Public API                                                   */
/* ---------------------------------------------------------------------------*/

int8_t MQTT_Init(MQTT_Handle_t *handle, ESP8266_Handle_t *esp_handle)
{
    if (handle == NULL || esp_handle == NULL)
    {
        return MQTT_ERR_CONNECT;
    }

    memset(handle, 0, sizeof(MQTT_Handle_t));

    handle->esp              = esp_handle;
    handle->state             = MQTT_STATE_DISCONNECTED;
    handle->last_packet_id    = MQTT_PACKET_ID_START;
    handle->last_keepalive_ms = 0;
    handle->initialized       = 1;

    return MQTT_OK;
}

int8_t MQTT_Connect(MQTT_Handle_t *handle,
                    const char *broker,
                    uint16_t port,
                    const char *client_id,
                    const char *user,
                    const char *pass)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return MQTT_ERR_CONNECT;
    }

    if (broker == NULL || client_id == NULL)
    {
        return MQTT_ERR_CONNECT;
    }

    handle->state = MQTT_STATE_CONNECTING;

    /* ---- Establish TCP connection ---- */
    char cipstart_cmd[256];
    int32_t n = snprintf(cipstart_cmd, sizeof(cipstart_cmd),
                         "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                         broker, port);
    if (n < 0 || (uint32_t)n >= sizeof(cipstart_cmd))
    {
        handle->state = MQTT_STATE_DISCONNECTED;
        return MQTT_ERR_CONNECT;
    }

    int8_t ret = ESP8266_SendAT(handle->esp, cipstart_cmd, "OK",
                                ESP8266_LONG_TIMEOUT);
    if (ret != ESP8266_OK)
    {
        handle->state = MQTT_STATE_DISCONNECTED;
        return MQTT_ERR_CONNECT;
    }

    handle->esp->state = ESP8266_STATE_CONNECTED_TCP;

    /* ---- Build MQTT CONNECT packet ---- */
    uint8_t packet[256];
    uint16_t pos = 0;

    uint16_t client_id_len = strlen(client_id);
    uint16_t user_len      = strlen(user);
    uint16_t pass_len      = strlen(pass);

    /* Variable header: protocol name "MQTT" (4 bytes) */
    const uint8_t protocol_name[] = {0x00, 0x04, 'M', 'Q', 'T', 'T'};

    /* Protocol level (4 = MQTT 3.1.1) */
    uint8_t protocol_level = 0x04;

    /* Connect flags */
    uint8_t connect_flags = MQTT_CONNECT_FLAG_CLEAN;

    if (user_len > 0)
    {
        connect_flags |= 0x80; /* User name flag */
    }
    if (pass_len > 0)
    {
        connect_flags |= 0x40; /* Password flag */
    }

    /* Keepalive seconds (big-endian) */
    uint8_t keepalive_hi = (MQTT_KEEPALIVE_SEC >> 8) & 0xFF;
    uint8_t keepalive_lo =  MQTT_KEEPALIVE_SEC & 0xFF;

    /* Calculate remaining length */
    uint32_t remaining = sizeof(protocol_name)          /* Protocol name */
                       + 1                              /* Protocol level */
                       + 1                              /* Connect flags */
                       + 2                              /* Keepalive */
                       + 2 + client_id_len;             /* Client ID */

    if (user_len > 0)
    {
        remaining += 2 + user_len;
    }
    if (pass_len > 0)
    {
        remaining += 2 + pass_len;
    }

    /* Encode fixed header */
    uint8_t remlen_buf[MQTT_MAX_REMLEN_BYTES];
    uint8_t remlen_bytes = MQTT_EncodeRemLen(remaining, remlen_buf);

    packet[pos++] = MQTT_PKT_CONNECT;                    /* Fixed header */
    for (uint8_t i = 0; i < remlen_bytes; i++)
    {
        packet[pos++] = remlen_buf[i];
    }

    /* Variable header */
    for (uint8_t i = 0; i < sizeof(protocol_name); i++)
    {
        packet[pos++] = protocol_name[i];
    }
    packet[pos++] = protocol_level;
    packet[pos++] = connect_flags;
    packet[pos++] = keepalive_hi;
    packet[pos++] = keepalive_lo;

    /* Payload: Client ID */
    packet[pos++] = (client_id_len >> 8) & 0xFF;
    packet[pos++] =  client_id_len & 0xFF;
    memcpy(&packet[pos], client_id, client_id_len);
    pos += client_id_len;

    /* Payload: User name */
    if (user_len > 0)
    {
        packet[pos++] = (user_len >> 8) & 0xFF;
        packet[pos++] =  user_len & 0xFF;
        memcpy(&packet[pos], user, user_len);
        pos += user_len;
    }

    /* Payload: Password */
    if (pass_len > 0)
    {
        packet[pos++] = (pass_len >> 8) & 0xFF;
        packet[pos++] =  pass_len & 0xFF;
        memcpy(&packet[pos], pass, pass_len);
        pos += pass_len;
    }

    /* Send CONNECT packet */
    ret = MQTT_SendPacket(handle, packet, pos);
    if (ret != MQTT_OK)
    {
        handle->state = MQTT_STATE_DISCONNECTED;
        return MQTT_ERR_CONNECT;
    }

    handle->state             = MQTT_STATE_CONNECTED;
    handle->last_keepalive_ms = HAL_GetTick();

    return MQTT_OK;
}

int8_t MQTT_Publish(MQTT_Handle_t *handle,
                    const char *topic,
                    const uint8_t *payload,
                    uint8_t qos)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return MQTT_ERR_PUBLISH;
    }

    if (topic == NULL || payload == NULL)
    {
        return MQTT_ERR_PUBLISH;
    }

    if (qos > MQTT_QOS2)
    {
        qos = MQTT_QOS0;
    }

    uint8_t packet[512];
    uint16_t pos = 0;

    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen((const char *)payload);

    /* Fixed header: QoS determines flags */
    uint8_t fixed_header = MQTT_PKT_PUBLISH | ((qos & 0x03) << 1);
    /* Retain = 0 */

    /* Remaining length: topic + optional packet ID + payload */
    uint32_t remaining = 2 + topic_len + payload_len;  /* Topic length field + topic + payload */
    if (qos > MQTT_QOS0)
    {
        remaining += 2; /* Packet identifier */
    }

    /* Encode fixed header */
    uint8_t remlen_buf[MQTT_MAX_REMLEN_BYTES];
    uint8_t remlen_bytes = MQTT_EncodeRemLen(remaining, remlen_buf);

    packet[pos++] = fixed_header;
    for (uint8_t i = 0; i < remlen_bytes; i++)
    {
        packet[pos++] = remlen_buf[i];
    }

    /* Topic */
    packet[pos++] = (topic_len >> 8) & 0xFF;
    packet[pos++] =  topic_len & 0xFF;
    memcpy(&packet[pos], topic, topic_len);
    pos += topic_len;

    /* Packet identifier (QoS > 0) */
    if (qos > MQTT_QOS0)
    {
        handle->last_packet_id++;
        packet[pos++] = (handle->last_packet_id >> 8) & 0xFF;
        packet[pos++] =  handle->last_packet_id & 0xFF;
    }

    /* Payload */
    memcpy(&packet[pos], payload, payload_len);
    pos += payload_len;

    return MQTT_SendPacket(handle, packet, pos);
}

int8_t MQTT_Subscribe(MQTT_Handle_t *handle,
                      const char *topic,
                      uint8_t qos)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return MQTT_ERR_SUBSCRIBE;
    }

    if (topic == NULL)
    {
        return MQTT_ERR_SUBSCRIBE;
    }

    if (qos > MQTT_QOS2)
    {
        qos = MQTT_QOS0;
    }

    uint8_t packet[256];
    uint16_t pos = 0;

    uint16_t topic_len = strlen(topic);

    /* Packet identifier */
    handle->last_packet_id++;
    uint16_t pkt_id = handle->last_packet_id;

    /* Remaining length: packet ID (2) + topic len (2) + topic + QoS (1) */
    uint32_t remaining = 2 + 2 + topic_len + 1;

    uint8_t remlen_buf[MQTT_MAX_REMLEN_BYTES];
    uint8_t remlen_bytes = MQTT_EncodeRemLen(remaining, remlen_buf);

    packet[pos++] = MQTT_PKT_SUBSCRIBE;
    for (uint8_t i = 0; i < remlen_bytes; i++)
    {
        packet[pos++] = remlen_buf[i];
    }

    /* Packet identifier */
    packet[pos++] = (pkt_id >> 8) & 0xFF;
    packet[pos++] =  pkt_id & 0xFF;

    /* Topic filter */
    packet[pos++] = (topic_len >> 8) & 0xFF;
    packet[pos++] =  topic_len & 0xFF;
    memcpy(&packet[pos], topic, topic_len);
    pos += topic_len;

    /* Requested QoS */
    packet[pos++] = qos & 0x03;

    return MQTT_SendPacket(handle, packet, pos);
}

int8_t MQTT_KeepAlive(MQTT_Handle_t *handle)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return MQTT_ERR_CONNECT;
    }

    if (handle->state != MQTT_STATE_CONNECTED)
    {
        return MQTT_ERR_CONNECT;
    }

    uint32_t now = HAL_GetTick();

    /* Send PINGREQ at half the keepalive interval to stay safe */
    if ((now - handle->last_keepalive_ms) < (uint32_t)(MQTT_KEEPALIVE_SEC * 500))
    {
        return MQTT_OK; /* No ping needed yet */
    }

    uint8_t pingreq[2] = {MQTT_PKT_PINGREQ, 0x00};

    int8_t ret = MQTT_SendPacket(handle, pingreq, sizeof(pingreq));
    if (ret == MQTT_OK)
    {
        handle->last_keepalive_ms = now;
    }

    return ret;
}

int8_t MQTT_Disconnect(MQTT_Handle_t *handle)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return MQTT_ERR_CONNECT;
    }

    handle->state = MQTT_STATE_DISCONNECTING;

    uint8_t disconnect[2] = {MQTT_PKT_DISCONNECT, 0x00};

    int8_t ret = MQTT_SendPacket(handle, disconnect, sizeof(disconnect));

    handle->state = MQTT_STATE_DISCONNECTED;

    return ret;
}
