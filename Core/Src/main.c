/**
 * @file    main.c
 * @brief   STM32 智能环境监测站 - 主程序入口
 * @author  Project SEMS Team
 * @version 1.0.0
 * @date    2026-07-04
 *
 * @details 系统初始化流程:
 *          1. HAL 库初始化
 *          2. 系统时钟配置 (168MHz)
 *          3. 外设初始化 (GPIO/I2C/SPI/UART/ADC/TIM)
 *          4. 外设驱动初始化 (传感器/显示/通信/存储)
 *          5. 创建 FreeRTOS 任务
 *          6. 启动调度器
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#include "sensors/dht22.h"
#include "sensors/bmp280.h"
#include "sensors/mq135.h"
#include "display/ssd1306.h"
#include "comm/esp8266.h"
#include "comm/mqtt.h"
#include "storage/sd_card.h"
#include "app/sensor_manager.h"
#include "app/data_logger.h"
#include "app/wifi_manager.h"
#include "app/alarm_manager.h"

/*=========================== 全局变量 ===========================*/

SensorData_t    g_sensor_data = {0};
volatile uint32_t g_sys_tick_ms = 0;
volatile uint8_t  g_wifi_connected = 0;
/* 全局句柄 (供其他模块引用) */
BMP280_Handle_t     bmp280_h = {0};
ESP8266_Handle_t    esp8266_h = {0};


/*=========================== 句柄定义 ===========================*/

/* 外设句柄 */
I2C_HandleTypeDef   hi2c1;     /* I2C1: BMP280 + SSD1306 */
UART_HandleTypeDef  huart1;    /* USART1: 调试串口 */
UART_HandleTypeDef  huart2;    /* USART2: ESP8266 */
SPI_HandleTypeDef   hspi1;     /* SPI1: SD 卡 */
ADC_HandleTypeDef   hadc1;     /* ADC1: MQ-135 + LDR */
TIM_HandleTypeDef   htim3;     /* TIM3: 蜂鸣器 PWM */
TIM_HandleTypeDef   htim4;     /* TIM4: RGB LED PWM */
DMA_HandleTypeDef   hdma_usart1_tx;
DMA_HandleTypeDef   hdma_usart2_tx;

/* FreeRTOS 内核对象 */
TaskHandle_t        xTaskSensorHandle   = NULL;
TaskHandle_t        xTaskDisplayHandle  = NULL;
TaskHandle_t        xTaskWiFiHandle     = NULL;
TaskHandle_t        xTaskStorageHandle  = NULL;
TaskHandle_t        xTaskAlarmHandle    = NULL;
TaskHandle_t        xTaskCLIHandle      = NULL;

QueueHandle_t       xQueueSensorData    = NULL;   /* 传感器数据队列 */
QueueHandle_t       xQueueAlarmEvent    = NULL;   /* 告警事件队列 */

SemaphoreHandle_t   xSemI2C1            = NULL;   /* I2C1 互斥 */
SemaphoreHandle_t   xSemSPI1            = NULL;   /* SPI1 互斥 */
SemaphoreHandle_t   xSemUSART2_TX       = NULL;   /* ESP8266 发送互斥 */
SemaphoreHandle_t   xSemSD_Insertion    = NULL;   /* SD 卡插入信号 */
SemaphoreHandle_t   xSemWiFi_Connected  = NULL;   /* WiFi 连接信号 */

/*=========================== 函数声明 ===========================*/

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_DMA_Init(void);

/* FreeRTOS 任务函数 */
static void Task_Sensor_Handler(void *pvParameters);
static void Task_Display_Handler(void *pvParameters);
static void Task_WiFi_Handler(void *pvParameters);
static void Task_Storage_Handler(void *pvParameters);
static void Task_Alarm_Handler(void *pvParameters);
static void Task_CLI_Handler(void *pvParameters);

/*=========================== 主函数 ===========================*/

/**
 * @brief  主函数入口
 * @note   STM32 启动文件调用此函数
 */
int main(void)
{
    /* HAL 库初始化 */
    HAL_Init();

    /* 系统时钟配置: HSE 8MHz -> PLL -> 168MHz */
    SystemClock_Config();

    /* 外设初始化 */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();

    /*========== 外设驱动初始化 ==========*/

    /* 调试串口欢迎信息 */
    HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n====================================\r\n", 38, 100);
    HAL_UART_Transmit(&huart1, (uint8_t *)" STM32 Smart Environmental Monitor v1.0\r\n", 42, 100);
    HAL_UART_Transmit(&huart1, (uint8_t *)"====================================\r\n", 38, 100);

    /* 初始化 DHT22 */
    if (DHT22_Init(GPIOB, GPIO_PIN_0) != DHT22_OK) {
        printf("[ERR] DHT22 init failed\r\n");
    }

    /* 初始化 BMP280 */
    BMP280_Handle_t bmp280;
    if (BMP280_Init(&bmp280, &hi2c1, BMP280_ADDR_CSB_LOW) != BMP280_OK) {
        printf("[ERR] BMP280 init failed\r\n");
    }

    /* 初始化 MQ-135 */
    if (MQ135_Init(&hadc1, ADC_CHANNEL_0) != MQ135_OK) {
        printf("[ERR] MQ135 init failed\r\n");
    }

    /* 初始化 SSD1306 OLED */
    if (SSD1306_Init(&hi2c1) != SSD1306_OK) {
        printf("[ERR] SSD1306 init failed\r\n");
    }
    SSD1306_Clear();
    SSD1306_DrawString(0, 0, "Booting...", &Font_7x10);
    SSD1306_UpdateScreen();

    /* 初始化 ESP8266 */
    ESP8266_Handle_t esp8266;
    ESP8266_Init(&esp8266, &huart2, 
                 ESP_RST_GPIO_Port, ESP_RST_Pin,
                 ESP_GPIO0_GPIO_Port, ESP_GPIO0_Pin);

    /* 初始化 SD 卡 (SPI 模式) */
    SD_Card_Init(&hspi1, SPI_CS_Pin);

    /*========== 创建 FreeRTOS 内核对象 ==========*/

    /* 数据队列 - 传感器数据从采集传给显示/存储/WiFi */
    xQueueSensorData = xQueueCreate(10, sizeof(SensorData_t));

    /* 告警队列 - 告警事件 */
    xQueueAlarmEvent = xQueueCreate(5, sizeof(AlarmEvent_t));

    /* 互斥信号量 */
    xSemI2C1           = xSemaphoreCreateMutex();
    xSemSPI1           = xSemaphoreCreateMutex();
    xSemUSART2_TX      = xSemaphoreCreateMutex();

    /* 二值信号量 */
    xSemSD_Insertion   = xSemaphoreCreateBinary();
    xSemWiFi_Connected = xSemaphoreCreateBinary();

    /*========== 创建 FreeRTOS 任务 ==========*/

    /* 任务: 传感器数据采集 (优先级最高) */
    xTaskCreate(Task_Sensor_Handler, "Sensor", 512, NULL, 4, &xTaskSensorHandle);

    /* 任务: OLED 显示刷新 */
    xTaskCreate(Task_Display_Handler, "Display", 1024, NULL, 2, &xTaskDisplayHandle);

    /* 任务: WiFi/MQTT 通信 */
    xTaskCreate(Task_WiFi_Handler, "WiFi", 1024, NULL, 2, &xTaskWiFiHandle);

    /* 任务: SD 卡数据存储 */
    xTaskCreate(Task_Storage_Handler, "Storage", 512, NULL, 1, &xTaskStorageHandle);

    /* 任务: 告警管理 */
    xTaskCreate(Task_Alarm_Handler, "Alarm", 256, NULL, 3, &xTaskAlarmHandle);

    /* 任务: 串口调试控制台 */
    xTaskCreate(Task_CLI_Handler, "CLI", 512, NULL, 1, &xTaskCLIHandle);

    /* 启动 FreeRTOS 调度器 */
    vTaskStartScheduler();

    /* 正常情况下不会执行到这里 */
    while (1) {
        Error_Handler();
    }
}

/*=========================== 系统时钟配置 ===========================*/

/**
 * @brief  系统时钟配置
 *         HSE 8MHz → PLL → SYSCLK 168MHz
 *         APB1: 42MHz, APB2: 84MHz
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSE 振荡器配置: 8MHz 外部晶振 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;           /* 8MHz / 8 = 1MHz */
    RCC_OscInitStruct.PLL.PLLN = 336;         /* 1MHz * 336 = 336MHz */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; /* 336MHz / 2 = 168MHz */
    RCC_OscInitStruct.PLL.PLLQ = 4;           /* 336MHz / 4 = 84MHz (USB/SDIO) */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* 时钟树配置 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK   | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;     /* HCLK = 168MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;      /* APB1 = 42MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;      /* APB2 = 84MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/*=========================== 外设初始化 ===========================*/

/**
 * @brief  GPIO 初始化
 *         - DHT22: PB0 (开漏输出)
 *         - SD_CD: PC0 (输入)
 *         - ESP_RST: PC1 (推挽输出)
 *         - ESP_GPIO0: PC2 (推挽输出)
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* DHT22 Data - PB0: 开漏输出, 无上拉 */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* SD 卡检测 - PC0: 输入上拉 (低电平=插入) */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ESP8266 控制 - PC1(RST), PC2(GPIO0): 推挽输出 */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* 默认状态: ESP8266 复位释放, 正常模式 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);   /* RST = HIGH */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);   /* GPIO0 = HIGH */
}

/**
 * @brief  I2C1 初始化: 用于 BMP280 + SSD1306
 *         SCL: PB6, SDA: PB7, 400kHz Fast Mode
 */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;           /* 400kHz Fast Mode */
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  USART1 初始化: 调试串口 (115200-8N1)
 *         TX: PA9, RX: PA10
 */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  USART2 初始化: ESP8266 通信 (115200-8N1)
 *         TX: PA2, RX: PA3
 */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  SPI1 初始化: SD 卡通信
 *         NSS: PA4, SCK: PA5, MISO: PA6, MOSI: PA7
 */
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 21MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  ADC1 初始化: MQ-135(IN0) + LDR(IN1)
 */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 2;     /* 2 个通道 */
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /* ADC 通道0: MQ-135 (空气质量) */
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    /* ADC 通道1: LDR (光照) */
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  TIM3 初始化: 蜂鸣器 PWM
 *         CH4: PB1, 1kHz PWM
 */
static void MX_TIM3_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 168 - 1;           /* 168MHz / 168 = 1MHz */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;             /* 1MHz / 1000 = 1kHz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;                      /* 初始占空比 0 (静音) */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  TIM4 初始化: RGB LED PWM
 *         CH1: PE0 (R), CH2: PE1 (G), CH3: PE2 (B)
 */
static void MX_TIM4_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 168 - 1;           /* 1MHz */
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 1000 - 1;             /* 1kHz */
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&htim4) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1);  /* R */
    HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2);  /* G */
    HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3);  /* B */
}

/**
 * @brief  DMA 初始化: USART1/USART2 TX
 */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA2 Stream7: USART1 TX */
    hdma_usart1_tx.Instance = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    /* DMA2 Stream2: USART2 TX */
    hdma_usart2_tx.Instance = DMA2_Stream2;
    hdma_usart2_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode = DMA_NORMAL;
    hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart2, hdmatx, hdma_usart2_tx);
}

/*=========================== FreeRTOS 任务实现 ===========================*/

/**
 * @brief  传感器采集任务
 * @note   定时采集各传感器数据，发送到数据队列
 */
static void Task_Sensor_Handler(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    SensorData_t data = {0};
    uint32_t loop_count = 0;

    /* MQ-135 预热等待 (首次) */
    vTaskDelay(pdMS_TO_TICKS(60000));
    printf("[INFO] MQ-135 warm-up complete\r\n");

    for (;;) {
        loop_count++;

        /* 采集温湿度 (每 2 次) */
        if (loop_count % 2 == 0) {
            float temp = 0, humid = 0;
            if (DHT22_Read(&temp, &humid) == DHT22_OK) {
                data.temperature = temp;
                data.humidity    = humid;
                data.valid_flags |= (SENSOR_FLAG_TEMP | SENSOR_FLAG_HUMID);
            }
        }

        /* 采集气压 (每 5 次) */
        if (loop_count % 5 == 0) {
            float press = 0, bmp_temp = 0;
            if (BMP280_ReadPressure(&bmp280_h, &press) == BMP280_OK) {
                data.pressure = press;
                data.valid_flags |= SENSOR_FLAG_PRESSURE;
            }
        }

        /* 采集空气质量 (每次) */
        {
            float ppm = 0;
            if (MQ135_ReadPPM(&ppm) == MQ135_OK) {
                data.air_quality_ppm = ppm;
                data.valid_flags |= SENSOR_FLAG_AIR_QUALITY;
            }
        }

        /* 采集光照 (每次) */
        {
            uint32_t adc_val;
            if (MQ135_ReadRaw(ADC_CHANNEL_1, &adc_val) == 0) {
                /* 10kΩ + LDR 分压, 近似 lux */
                data.light_lux = (float)adc_val / 4095.0f * 1000.0f;
                data.valid_flags |= SENSOR_FLAG_LIGHT;
            }
        }

        /* 更新时间戳 */
        data.timestamp = HAL_GetTick();

        /* 发送到数据队列 */
        if (xQueueSensorData != NULL) {
            xQueueOverwrite(xQueueSensorData, &data);
        }

        /* 更新时间到全局变量 (供其他任务直接读取) */
        g_sensor_data = data;

        /* RGB LED 状态指示: 正常采集 = 绿色呼吸 */
        RGB_LED_SetColor(0, 64, 0);  /* 微弱绿光 */

        /* 等待 1s */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief  OLED 显示任务
 * @note   从队列获取数据，更新 OLED 显示
 */
static void Task_Display_Handler(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    SensorData_t data;
    char line_buf[22];
    uint8_t page_index = 0;

    /* 显示首页: 启动完成 */
    SSD1306_Clear();
    SSD1306_DrawString(0, 0, "SEMS v1.0 Ready", &Font_11x18);
    SSD1306_DrawString(0, 20, "Waiting for data...", &Font_7x10);
    SSD1306_UpdateScreen();

    for (;;) {
        /* 从队列取最新数据 (非阻塞) */
        if (xQueuePeek(xQueueSensorData, &data, 0) == pdTRUE) {
            SSD1306_Clear();

            switch (page_index) {
                case 0: /* 温度 + 湿度 */
                    snprintf(line_buf, sizeof(line_buf), "Temp: %.1f C", data.temperature);
                    SSD1306_DrawString(0, 0, line_buf, &Font_11x18);
                    snprintf(line_buf, sizeof(line_buf), "Humi: %.1f %%", data.humidity);
                    SSD1306_DrawString(0, 22, line_buf, &Font_11x18);
                    snprintf(line_buf, sizeof(line_buf), "Updated: %lu", data.timestamp / 1000);
                    SSD1306_DrawString(0, 44, line_buf, &Font_7x10);
                    break;

                case 1: /* 气压 + 高度 */
                    snprintf(line_buf, sizeof(line_buf), "Press: %.1f hPa", data.pressure);
                    SSD1306_DrawString(0, 0, line_buf, &Font_11x18);
                    {
                        /* 标准气压 1013.25hPa, 近似海拔 */
                        float altitude = 44330.0f * (1.0f - powf(data.pressure / 1013.25f, 0.1903f));
                        snprintf(line_buf, sizeof(line_buf), "Alt: %.0f m", (double)altitude);
                    }
                    SSD1306_DrawString(0, 22, line_buf, &Font_11x18);
                    break;

                case 2: /* 空气质量 + 光照 */
                    snprintf(line_buf, sizeof(line_buf), "AQI: %.0f ppm", data.air_quality_ppm);
                    SSD1306_DrawString(0, 0, line_buf, &Font_11x18);
                    snprintf(line_buf, sizeof(line_buf), "Light: %.0f lux", data.light_lux);
                    SSD1306_DrawString(0, 22, line_buf, &Font_11x18);
                    break;

                case 3: /* WiFi 状态 */
                    SSD1306_DrawString(0, 0, "WiFi Status:", &Font_7x10);
                    if (g_wifi_connected) {
                        SSD1306_DrawString(0, 16, "Connected", &Font_11x18);
                        SSD1306_DrawString(0, 38, "MQTT: Active", &Font_7x10);
                    } else {
                        SSD1306_DrawString(0, 16, "Disconnected", &Font_11x18);
                        SSD1306_DrawString(0, 38, "Reconnecting...", &Font_7x10);
                    }
                    break;
            }

            SSD1306_UpdateScreen();
            page_index = (page_index + 1) % 4;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DISPLAY_REFRESH_INTERVAL));
    }
}

/**
 * @brief  WiFi/MQTT 通信任务
 * @note   连接 WiFi, 定期上报传感器数据到 MQTT Broker
 */
static void Task_WiFi_Handler(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    SensorData_t data;
    char json_buf[MQTT_JSON_BUF_SIZE];
    uint8_t retry_count = 0;

    /* 初始化 ESP8266 */
    if (ESP8266_Init(&esp8266_h) != ESP8266_OK) {
        printf("[ERR] ESP8266 init failed\r\n");
    }

    for (;;) {
        /* 尝试连接 WiFi */
        if (!g_wifi_connected) {
            if (ESP8266_ConnectAP(ESP8266_SSID, ESP8266_PASS) == ESP8266_OK) {
                g_wifi_connected = 1;
                printf("[INFO] WiFi connected\r\n");

                /* 连接 MQTT Broker */
                if (MQTT_Connect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID, 
                                 MQTT_USER, MQTT_PASS) == MQTT_OK) {
                    printf("[INFO] MQTT connected\r\n");
                    xSemaphoreGive(xSemWiFi_Connected);
                }
            } else {
                retry_count++;
                /* 指数退避: 1s, 2s, 4s, ... 最长 60s */
                uint32_t delay = (1 << (retry_count > 6 ? 6 : retry_count)) * 1000;
                printf("[WARN] WiFi retry %d, wait %lu ms\r\n", retry_count, delay);
                vTaskDelay(pdMS_TO_TICKS(delay));
                continue;
            }
        }

        /* 读取最新传感器数据并上报 */
        if (xQueuePeek(xQueueSensorData, &data, 0) == pdTRUE) {
            /* 构建 JSON 负载 */
            snprintf(json_buf, sizeof(json_buf),
                "{"
                "\"id\":\"%s\","
                "\"ts\":%lu,"
                "\"temp\":%.1f,"
                "\"humid\":%.1f,"
                "\"press\":%.1f,"
                "\"aqi\":%.0f,"
                "\"light\":%.0f"
                "}",
                MQTT_CLIENT_ID, data.timestamp,
                (double)data.temperature, (double)data.humidity,
                (double)data.pressure, (double)data.air_quality_ppm,
                (double)data.light_lux);

            /* 发布到 MQTT Topic */
            if (MQTT_Publish(MQTT_TOPIC_DATA, json_buf, 0) != MQTT_OK) {
                printf("[ERR] MQTT publish failed\r\n");
                g_wifi_connected = 0;
            } else {
                printf("[INFO] Data published: %s\r\n", json_buf);
            }
        }

        /* MQTT Keep-Alive */
        MQTT_KeepAlive();

        /* 等待 30s 后再次上报 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(WIFI_UPLOAD_INTERVAL));
    }
}

/**
 * @brief  SD 卡存储任务
 * @note   定期将传感器数据写入 SD 卡 CSV 文件
 */
static void Task_Storage_Handler(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    SensorData_t data;
    char csv_line[128];

    for (;;) {
        /* 检查 SD 卡是否插入 */
        if (HAL_GPIO_ReadPin(SD_CD_GPIO_Port, SD_CD_Pin) == GPIO_RESET) {
            if (xSemaphoreTake(xSemSPI1, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (SD_Card_Mount() == SD_OK) {
                    /* 读取最新数据并写入 CSV */
                    if (xQueuePeek(xQueueSensorData, &data, 0) == pdTRUE) {
                        snprintf(csv_line, sizeof(csv_line),
                            "%lu,%.1f,%.1f,%.1f,%.0f,%.0f\r\n",
                            data.timestamp,
                            (double)data.temperature, (double)data.humidity,
                            (double)data.pressure, (double)data.air_quality_ppm,
                            (double)data.light_lux);

                        SD_Card_AppendFile("sems_log.csv", csv_line);
                    }
                    SD_Card_Unmount();
                }
                xSemaphoreGive(xSemSPI1);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(STORAGE_LOG_INTERVAL));
    }
}

/**
 * @brief  告警管理任务
 * @note   检查传感器数据是否超限，触发告警
 */
static void Task_Alarm_Handler(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    SensorData_t data;
    AlarmEvent_t alarm;
    uint8_t alarm_active = 0;

    for (;;) {
        if (xQueuePeek(xQueueSensorData, &data, 0) == pdTRUE) {
            /* 检查各传感器阈值 */
            if (data.valid_flags & SENSOR_FLAG_TEMP) {
                if (data.temperature > ALARM_TEMP_HIGH_DEFAULT) {
                    alarm.type = ALARM_TYPE_TEMP_HIGH;
                    alarm.severity = ALARM_LEVEL_WARNING;
                    alarm.current_value = data.temperature;
                    alarm.threshold = ALARM_TEMP_HIGH_DEFAULT;
                    xQueueSend(xQueueAlarmEvent, &alarm, 0);
                }
            }

            if (data.valid_flags & SENSOR_FLAG_AIR_QUALITY) {
                if (data.air_quality_ppm > ALARM_AQI_HIGH_DEFAULT) {
                    alarm.type = ALARM_TYPE_AQI_HIGH;
                    alarm.severity = ALARM_LEVEL_CRITICAL;
                    alarm.current_value = data.air_quality_ppm;
                    alarm.threshold = ALARM_AQI_HIGH_DEFAULT;
                    xQueueSend(xQueueAlarmEvent, &alarm, 0);
                }
            }
        }

        /* 处理告警队列 */
        if (xQueueReceive(xQueueAlarmEvent, &alarm, 0) == pdTRUE) {
            alarm_active = 1;

            switch (alarm.severity) {
                case ALARM_LEVEL_CRITICAL:
                    /* 蜂鸣器响 + 红灯 */
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 500);
                    RGB_LED_SetColor(255, 0, 0);
                    printf("[ALARM] CRITICAL: type=%d val=%.1f\r\n", 
                           alarm.type, (double)alarm.current_value);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
                    break;

                case ALARM_LEVEL_WARNING:
                    /* 黄灯 + 日志 */
                    RGB_LED_SetColor(128, 128, 0);
                    printf("[ALARM] WARNING: type=%d val=%.1f\r\n",
                           alarm.type, (double)alarm.current_value);
                    break;

                default:
                    break;
            }
        } else {
            if (alarm_active) {
                /* 无告警时恢复绿灯 */
                RGB_LED_SetColor(0, 64, 0);
                alarm_active = 0;
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

/**
 * @brief  串口调试控制台任务
 * @note   通过 USART1 接收简单命令并执行
 */
static void Task_CLI_Handler(void *pvParameters)
{
    (void)pvParameters;
    uint8_t rx_byte;
    char cmd_buf[64];
    uint8_t cmd_idx = 0;

    printf("\r\nSEMS CLI Ready\r\n");
    printf("Commands: status, reset, wifi, help\r\n> ");

    for (;;) {
        if (HAL_UART_Receive(&huart1, &rx_byte, 1, pdMS_TO_TICKS(100)) == HAL_OK) {
            if (rx_byte == '\r' || rx_byte == '\n') {
                cmd_buf[cmd_idx] = '\0';
                printf("\r\n");

                if (strcmp((char *)cmd_buf, "status") == 0) {
                    printf("Temp: %.1f C\r\n", (double)g_sensor_data.temperature);
                    printf("Humid: %.1f %%\r\n", (double)g_sensor_data.humidity);
                    printf("Press: %.1f hPa\r\n", (double)g_sensor_data.pressure);
                    printf("AQI: %.0f ppm\r\n", (double)g_sensor_data.air_quality_ppm);
                    printf("Light: %.0f lux\r\n", (double)g_sensor_data.light_lux);
                    printf("WiFi: %s\r\n", g_wifi_connected ? "Connected" : "Disconnected");
                } else if (strcmp((char *)cmd_buf, "reset") == 0) {
                    printf("Resetting...\r\n");
                    HAL_Delay(100);
                    NVIC_SystemReset();
                } else if (strcmp((char *)cmd_buf, "wifi") == 0) {
                    g_wifi_connected = 0;
                    printf("WiFi reconnecting...\r\n");
                } else if (strcmp((char *)cmd_buf, "help") == 0) {
                    printf("Available commands:\r\n");
                    printf("  status   - Show sensor status\r\n");
                    printf("  reset    - Reset MCU\r\n");
                    printf("  wifi     - Reconnect WiFi\r\n");
                    printf("  help     - Show this help\r\n");
                } else if (cmd_idx > 0) {
                    printf("Unknown: %s\r\n", cmd_buf);
                }

                printf("> ");
                cmd_idx = 0;
                memset(cmd_buf, 0, sizeof(cmd_buf));
            } else if (rx_byte == '\b' && cmd_idx > 0) {
                cmd_idx--;
                printf("\b \b");
            } else if (rx_byte >= ' ' && rx_byte <= '~' && cmd_idx < sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_idx++] = rx_byte;
                HAL_UART_Transmit(&huart1, &rx_byte, 1, 10);
            }
        }
    }
}

/*=========================== 辅助函数 ===========================*/

/**
 * @brief  RGB LED 颜色设置
 * @param  r  红色亮度 0-255
 * @param  g  绿色亮度 0-255
 * @param  b  蓝色亮度 0-255
 */
void RGB_LED_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, r);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, g);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, b);
}

/**
 * @brief  错误处理函数
 * @note   系统严重错误时进入死循环
 */
void Error_Handler(void)
{
    __disable_irq();
    RGB_LED_SetColor(255, 0, 0);  /* 红灯常亮 */
    printf("[FATAL] System error, halting\r\n");
    while (1) {
        /* 等待看门狗复位或调试器介入 */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("[ASSERT] %s:%lu\r\n", file, line);
}
#endif

