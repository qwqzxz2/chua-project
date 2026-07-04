/**
 * @file    esp8266.c
 * @brief   ESP8266 WiFi module AT command driver implementation
 */

/* Includes ------------------------------------------------------------------*/
#include "esp8266.h"

/* Private defines -----------------------------------------------------------*/

/** Ring buffer mask */
#define RX_BUF_MASK  (ESP8266_RX_BUF_SIZE - 1)

/** AT command response terminators */
#define RESP_OK     "OK"
#define RESP_ERROR  "ERROR"
#define RESP_FAIL   "FAIL"
#define RESP_SEND_OK "SEND OK"

/** AT+CIPSTART protocol and domain format */
#define TCP_FORMAT  "TCP"

/* Private variables ---------------------------------------------------------*/

/** Pointer to the active handle used by UART RX callback */
static ESP8266_Handle_t *active_handle = NULL;

/* Private function prototypes ----------------------------------------------*/
static void     ESP8266_UARTRxCallback(ESP8266_Handle_t *handle, uint8_t byte);
static int8_t   ESP8266_WaitResponse(ESP8266_Handle_t *handle,
                                     const char *expected,
                                     uint32_t timeout_ms);

/* ---------------------------------------------------------------------------*/
/*               UART / Ring Buffer Helpers                                   */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  UART RX character callback (call from HAL_UART_RxCpltCallback or
 *         idle-line interrupt)
 * @param  handle Pointer to ESP8266 handle
 * @param  byte   Received byte
 */
static void ESP8266_UARTRxCallback(ESP8266_Handle_t *handle, uint8_t byte)
{
    uint16_t next = (handle->rx_index + 1) & RX_BUF_MASK;

    if (next != handle->rx_processed)
    {
        handle->rx_buf[handle->rx_index] = byte;
        handle->rx_index = next;
    }
}

/**
 * @brief  Read one byte from the ring buffer
 * @param  handle Pointer to ESP8266 handle
 * @param  byte   Output byte
 * @retval 1 if byte read, 0 if buffer empty
 */
static uint8_t ESP8266_RingBufRead(ESP8266_Handle_t *handle, uint8_t *byte)
{
    if (handle->rx_processed == handle->rx_index)
    {
        return 0;
    }

    *byte = handle->rx_buf[handle->rx_processed];
    handle->rx_processed = (handle->rx_processed + 1) & RX_BUF_MASK;
    return 1;
}

/**
 * @brief  Flush the ring buffer
 * @param  handle Pointer to ESP8266 handle
 */
static void ESP8266_RingBufFlush(ESP8266_Handle_t *handle)
{
    handle->rx_index     = 0;
    handle->rx_processed = 0;
    memset(handle->resp_buf, 0, sizeof(handle->resp_buf));
    handle->resp_ready   = 0;
    handle->resp_timeout = 0;
}

/* ---------------------------------------------------------------------------*/
/*               UART Transmit / Receive                                       */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Transmit data over UART (blocking)
 * @param  handle Pointer to ESP8266 handle
 * @param  data   Data buffer
 * @param  len    Number of bytes
 * @retval ESP8266_OK on success
 */
static int8_t ESP8266_UART_Transmit(ESP8266_Handle_t *handle,
                                    const uint8_t *data,
                                    uint16_t len)
{
    if (HAL_UART_Transmit(handle->huart, (uint8_t *)data, len,
                          HAL_MAX_DELAY) != HAL_OK)
    {
        return ESP8266_ERR_AT;
    }
    return ESP8266_OK;
}

/**
 * @brief  Start UART receive in interrupt mode (one byte at a time)
 * @param  handle Pointer to ESP8266 handle
 * @retval ESP8266_OK on success
 */
static int8_t ESP8266_UART_ReceiveIT(ESP8266_Handle_t *handle)
{
    if (HAL_UART_Receive_IT(handle->huart,
                            &handle->huart->RxXferSize, /* dummy – real byte */
                            1) != HAL_OK)
    {
        return ESP8266_ERR_AT;
    }
    return ESP8266_OK;
}

/* Public: call this from HAL_UART_RxCpltCallback */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (active_handle != NULL && huart == active_handle->huart)
    {
        /* Re-arm single-byte RX */
        HAL_UART_Receive_IT(huart, &huart->RxXferSize, 1);
    }
}

/* ---------------------------------------------------------------------------*/
/*               Response Parser                                              */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  Wait for a specific expected response string with timeout
 * @param  handle     Pointer to ESP8266 handle
 * @param  expected   Expected substring to find in response
 * @param  timeout_ms Timeout in milliseconds
 * @retval ESP8266_OK if found, ESP8266_ERR_TIMEOUT on timeout,
 *         ESP8266_ERR_AT if ERROR/FAIL found
 */
static int8_t ESP8266_WaitResponse(ESP8266_Handle_t *handle,
                                   const char *expected,
                                   uint32_t timeout_ms)
{
    uint32_t tick_start = HAL_GetTick();
    uint16_t pos = 0;
    uint8_t  byte;

    memset(handle->resp_buf, 0, sizeof(handle->resp_buf));

    while ((HAL_GetTick() - tick_start) < timeout_ms)
    {
        while (ESP8266_RingBufRead(handle, &byte))
        {
            if (pos < sizeof(handle->resp_buf) - 1)
            {
                handle->resp_buf[pos++] = (char)byte;
                handle->resp_buf[pos]   = '\0';
            }

            /* Check for ERROR / FAIL – immediate failure */
            if (strstr((const char *)handle->resp_buf, RESP_ERROR) != NULL ||
                strstr((const char *)handle->resp_buf, RESP_FAIL)   != NULL)
            {
                return ESP8266_ERR_AT;
            }

            /* Check for expected response */
            if (strstr((const char *)handle->resp_buf, expected) != NULL)
            {
                return ESP8266_OK;
            }
        }
    }

    return ESP8266_ERR_TIMEOUT;
}

/* ---------------------------------------------------------------------------*/
/*               Public API                                                   */
/* ---------------------------------------------------------------------------*/

int8_t ESP8266_Init(ESP8266_Handle_t *handle,
                    UART_HandleTypeDef *huart,
                    GPIO_TypeDef *rst_port,
                    uint16_t rst_pin,
                    GPIO_TypeDef *gpio0_port,
                    uint16_t gpio0_pin)
{
    if (handle == NULL || huart == NULL)
    {
        return ESP8266_ERR_UNINIT;
    }

    memset(handle, 0, sizeof(ESP8266_Handle_t));

    handle->huart      = huart;
    handle->rst_port   = rst_port;
    handle->rst_pin    = rst_pin;
    handle->gpio0_port = gpio0_port;
    handle->gpio0_pin  = gpio0_pin;
    handle->state      = ESP8266_STATE_IDLE;
    handle->initialized = 1;

    active_handle = handle;

    /* Configure RST and GPIO0 as outputs */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    if (rst_port != NULL)
    {
        gpio.Pin = rst_pin;
        HAL_GPIO_Init(rst_port, &gpio);
        HAL_GPIO_WritePin(rst_port, rst_pin, GPIO_PIN_SET);  /* RST high */
    }

    if (gpio0_port != NULL)
    {
        gpio.Pin = gpio0_pin;
        HAL_GPIO_Init(gpio0_port, &gpio);
        HAL_GPIO_WritePin(gpio0_port, gpio0_pin, GPIO_PIN_SET); /* GPIO0 high */
    }

    /* Start UART interrupt reception */
    HAL_UART_Receive_IT(handle->huart, &handle->huart->RxXferSize, 1);

    /* Perform initial reset sequence */
    return ESP8266_Reset(handle);
}

int8_t ESP8266_Reset(ESP8266_Handle_t *handle)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return ESP8266_ERR_UNINIT;
    }

    handle->state = ESP8266_STATE_RESETTING;
    ESP8266_RingBufFlush(handle);

    /* RST low for 100 ms */
    if (handle->rst_port != NULL)
    {
        HAL_GPIO_WritePin(handle->rst_port, handle->rst_pin, GPIO_PIN_RESET);
    }
    HAL_Delay(100);

    /* RST high */
    if (handle->rst_port != NULL)
    {
        HAL_GPIO_WritePin(handle->rst_port, handle->rst_pin, GPIO_PIN_SET);
    }

    /* Wait for module to boot */
    HAL_Delay(1000);

    /* Flush any boot-up garbage */
    ESP8266_RingBufFlush(handle);

    /* Verify module responds to AT */
    int8_t ret = ESP8266_SendAT(handle, "AT\r\n", RESP_OK, ESP8266_DEFAULT_TIMEOUT);
    if (ret == ESP8266_OK)
    {
        handle->state = ESP8266_STATE_AT_READY;
    }
    else
    {
        handle->state = ESP8266_STATE_ERROR;
    }

    return ret;
}

int8_t ESP8266_SendAT(ESP8266_Handle_t *handle,
                      const char *cmd,
                      const char *expected_resp,
                      uint32_t timeout_ms)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return ESP8266_ERR_UNINIT;
    }

    if (cmd == NULL || expected_resp == NULL)
    {
        return ESP8266_ERR_AT;
    }

    /* Flush before sending command */
    ESP8266_RingBufFlush(handle);

    /* Transmit AT command */
    if (ESP8266_UART_Transmit(handle,
                              (const uint8_t *)cmd,
                              strlen(cmd)) != ESP8266_OK)
    {
        return ESP8266_ERR_AT;
    }

    /* Wait for expected response */
    return ESP8266_WaitResponse(handle, expected_resp, timeout_ms);
}

int8_t ESP8266_ConnectAP(ESP8266_Handle_t *handle,
                         const char *ssid,
                         const char *pass)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return ESP8266_ERR_UNINIT;
    }

    if (ssid == NULL || pass == NULL)
    {
        return ESP8266_ERR_AT;
    }

    int8_t ret;

    handle->state = ESP8266_STATE_CONNECTING_AP;

    /* 1. Set station mode */
    ret = ESP8266_SendAT(handle, "AT+CWMODE=1\r\n", RESP_OK,
                         ESP8266_DEFAULT_TIMEOUT);
    if (ret != ESP8266_OK)
    {
        handle->state = ESP8266_STATE_ERROR;
        return ret;
    }

    /* 2. Connect to AP */
    char cwjap_cmd[256];
    int32_t n = snprintf(cwjap_cmd, sizeof(cwjap_cmd),
                         "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pass);

    if (n < 0 || (uint32_t)n >= sizeof(cwjap_cmd))
    {
        handle->state = ESP8266_STATE_ERROR;
        return ESP8266_ERR_AT;
    }

    ret = ESP8266_SendAT(handle, cwjap_cmd, RESP_OK, ESP8266_LONG_TIMEOUT);
    if (ret != ESP8266_OK)
    {
        handle->state = ESP8266_STATE_ERROR;
        return ret;
    }

    /* 3. Get local IP to confirm connectivity */
    ret = ESP8266_SendAT(handle, "AT+CIFSR\r\n", RESP_OK,
                         ESP8266_DEFAULT_TIMEOUT);
    if (ret != ESP8266_OK)
    {
        handle->state = ESP8266_STATE_ERROR;
        return ret;
    }

    handle->state = ESP8266_STATE_CONNECTED_AP;
    return ESP8266_OK;
}

int8_t ESP8266_Disconnect(ESP8266_Handle_t *handle)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return ESP8266_ERR_UNINIT;
    }

    handle->state = ESP8266_STATE_DISCONNECTING;

    int8_t ret = ESP8266_SendAT(handle, "AT+CWQAP\r\n", RESP_OK,
                                ESP8266_DEFAULT_TIMEOUT);
    if (ret == ESP8266_OK)
    {
        handle->state = ESP8266_STATE_IDLE;
    }
    else
    {
        handle->state = ESP8266_STATE_ERROR;
    }

    return ret;
}

int8_t ESP8266_TCPSend(ESP8266_Handle_t *handle,
                       const uint8_t *data,
                       uint16_t len)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return ESP8266_ERR_UNINIT;
    }

    if (data == NULL || len == 0)
    {
        return ESP8266_ERR_AT;
    }

    /* AT+CIPSEND=<len> */
    char cipsend_cmd[32];
    int32_t n = snprintf(cipsend_cmd, sizeof(cipsend_cmd),
                         "AT+CIPSEND=%u\r\n", len);

    if (n < 0 || (uint32_t)n >= sizeof(cipsend_cmd))
    {
        return ESP8266_ERR_AT;
    }

    /* Send CIPSTART / CIPSEND and wait for ">" prompt */
    int8_t ret = ESP8266_SendAT(handle, cipsend_cmd, ">",
                                ESP8266_DEFAULT_TIMEOUT);
    if (ret != ESP8266_OK)
    {
        return ret;
    }

    /* Send raw data */
    ret = ESP8266_UART_Transmit(handle, data, len);
    if (ret != ESP8266_OK)
    {
        return ret;
    }

    /* Wait for SEND OK */
    return ESP8266_WaitResponse(handle, RESP_SEND_OK,
                                ESP8266_DEFAULT_TIMEOUT);
}
