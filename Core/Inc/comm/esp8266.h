/**
 * @file    esp8266.h
 * @brief   ESP8266 WiFi module AT command driver for STM32
 */

#ifndef __ESP8266_H__
#define __ESP8266_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/* Exported defines ----------------------------------------------------------*/

/** @defgroup ESP8266_Status Status codes
 *  @{
 */
#define ESP8266_OK           (0)
#define ESP8266_ERR_TIMEOUT  (-1)
#define ESP8266_ERR_AT       (-2)
#define ESP8266_ERR_UNINIT   (-3)
/** @} */

/** @defgroup ESP8266_Buffer Buffer size constants
 *  @{
 */
#define ESP8266_RX_BUF_SIZE  (512)
#define ESP8266_TX_BUF_SIZE  (512)
#define ESP8266_RESP_BUF_SIZE (512)
/** @} */

/** @defgroup ESP8266_Timeout Default timeout constants (ms)
 *  @{
 */
#define ESP8266_DEFAULT_TIMEOUT   (5000)
#define ESP8266_LONG_TIMEOUT      (15000)
#define ESP8266_SHORT_TIMEOUT     (1000)
/** @} */

/* Exported types ------------------------------------------------------------*/

/**
 * @brief ESP8266 WiFi connection states
 */
typedef enum
{
    ESP8266_STATE_IDLE       = 0,
    ESP8266_STATE_RESETTING,
    ESP8266_STATE_AT_READY,
    ESP8266_STATE_CONNECTING_AP,
    ESP8266_STATE_CONNECTED_AP,
    ESP8266_STATE_CONNECTING_TCP,
    ESP8266_STATE_CONNECTED_TCP,
    ESP8266_STATE_DISCONNECTING,
    ESP8266_STATE_ERROR
} ESP8266_State_t;

/**
 * @brief ESP8266 handle structure
 */
typedef struct
{
    UART_HandleTypeDef *huart;          /**< UART handle for communication */
    GPIO_TypeDef       *rst_port;       /**< Reset (RST) pin GPIO port */
    uint16_t            rst_pin;        /**< Reset (RST) pin number */
    GPIO_TypeDef       *gpio0_port;     /**< GPIO0 pin GPIO port */
    uint16_t            gpio0_pin;      /**< GPIO0 pin number */

    uint8_t             rx_buf[ESP8266_RX_BUF_SIZE];    /**< DMA receive buffer */
    volatile uint16_t   rx_index;       /**< Ring buffer write index */
    volatile uint16_t   rx_processed;   /**< Ring buffer read index */
    uint8_t             resp_buf[ESP8266_RESP_BUF_SIZE]; /**< Response buffer */
    volatile uint8_t    resp_ready;     /**< Flag: response fully received */
    volatile uint8_t    resp_timeout;   /**< Flag: response timed out */

    ESP8266_State_t     state;          /**< Current WiFi connection state */
    uint8_t             initialized;    /**< Non-zero if initialized */
} ESP8266_Handle_t;

/* Exported function prototypes ----------------------------------------------*/

/**
 * @brief  Initialize the ESP8266 module
 * @param  handle   Pointer to ESP8266 handle
 * @param  huart    Pointer to UART handle
 * @param  rst_port RST pin GPIO port
 * @param  rst_pin  RST pin number
 * @param  gpio0_port GPIO0 pin GPIO port
 * @param  gpio0_pin  GPIO0 pin number
 * @retval ESP8266_OK on success, else error code
 */
int8_t ESP8266_Init(ESP8266_Handle_t *handle,
                    UART_HandleTypeDef *huart,
                    GPIO_TypeDef *rst_port,
                    uint16_t rst_pin,
                    GPIO_TypeDef *gpio0_port,
                    uint16_t gpio0_pin);

/**
 * @brief  Hardware reset the ESP8266 module (RST low 100ms -> high)
 * @param  handle Pointer to ESP8266 handle
 * @retval ESP8266_OK on success, else error code
 */
int8_t ESP8266_Reset(ESP8266_Handle_t *handle);

/**
 * @brief  Send an AT command and wait for expected response
 * @param  handle        Pointer to ESP8266 handle
 * @param  cmd           AT command string (null-terminated)
 * @param  expected_resp Expected response substring (null-terminated)
 * @param  timeout_ms    Timeout in milliseconds
 * @retval ESP8266_OK if expected_resp found, ESP8266_ERR_TIMEOUT on timeout,
 *         ESP8266_ERR_AT on error response
 */
int8_t ESP8266_SendAT(ESP8266_Handle_t *handle,
                      const char *cmd,
                      const char *expected_resp,
                      uint32_t timeout_ms);

/**
 * @brief  Connect to a WiFi access point
 * @param  handle Pointer to ESP8266 handle
 * @param  ssid   Access point SSID (null-terminated)
 * @param  pass   Access point password (null-terminated)
 * @retval ESP8266_OK on success, else error code
 */
int8_t ESP8266_ConnectAP(ESP8266_Handle_t *handle,
                         const char *ssid,
                         const char *pass);

/**
 * @brief  Disconnect from the current WiFi access point
 * @param  handle Pointer to ESP8266 handle
 * @retval ESP8266_OK on success, else error code
 */
int8_t ESP8266_Disconnect(ESP8266_Handle_t *handle);

/**
 * @brief  Send data over the established TCP connection
 * @param  handle Pointer to ESP8266 handle
 * @param  data   Data buffer to send
 * @param  len    Length of data to send
 * @retval ESP8266_OK on success, else error code
 */
int8_t ESP8266_TCPSend(ESP8266_Handle_t *handle,
                       const uint8_t *data,
                       uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __ESP8266_H__ */
