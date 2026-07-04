/**
 * @file    sd_card.c
 * @brief   SD 卡驱动 (SPI + FATFS 简化实现)
 *          基于 STM32 HAL SPI 驱动 + FatFs 文件系统
 */
#include "storage/sd_card.h"
#include "ff.h"              /* FatFs 头文件 */
#include "diskio.h"          /* FatFs 底层接口 */
#include <string.h>

static SPI_HandleTypeDef *sd_spi = NULL;
static uint16_t           sd_cs_pin = 0;
static FATFS              sd_fatfs;
static uint8_t            sd_mounted = 0;
static uint8_t            sd_initialized = 0;

/* SPI 片选控制 */
#define SD_CS_LOW()  HAL_GPIO_WritePin(GPIOA, sd_cs_pin, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(GPIOA, sd_cs_pin, GPIO_PIN_SET)

void SD_Card_Init(SPI_HandleTypeDef *spi, uint16_t cs_pin)
{
    sd_spi = spi;
    sd_cs_pin = cs_pin;
    sd_initialized = 1;

    /* 片选设置为推挽输出 */
    GPIO_InitTypeDef gpio = {
        .Pin = cs_pin,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH
    };
    HAL_GPIO_Init(GPIOA, &gpio);
    SD_CS_HIGH();  /* 取消选中 */
}

int SD_Card_Mount(void)
{
    if (!sd_initialized) return SD_ERR_NOT_INIT;

    /* 挂载 FATFS */
    FRESULT fr = f_mount(&sd_fatfs, "0:", 1);
    if (fr != FR_OK) {
        sd_mounted = 0;
        return SD_ERR_MOUNT;
    }

    sd_mounted = 1;
    return SD_OK;
}

void SD_Card_Unmount(void)
{
    if (sd_mounted) {
        f_mount(NULL, "0:", 0);
        sd_mounted = 0;
    }
}

int SD_Card_AppendFile(const char *filename, const char *data)
{
    if (!sd_mounted) return SD_ERR_MOUNT;

    FIL fil;
    FRESULT fr;

    /* 以追加方式打开文件 */
    fr = f_open(&fil, filename, FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        /* 文件不存在则新建并写入 CSV 表头 */
        fr = f_open(&fil, filename, FA_CREATE_NEW | FA_WRITE);
        if (fr != FR_OK) return SD_ERR_OPEN;

        const char *header = "timestamp,temperature,humidity,pressure,aqi,light\n";
        UINT bw;
        f_write(&fil, header, strlen(header), &bw);
        f_close(&fil);

        /* 再次以追加方式打开 */
        fr = f_open(&fil, filename, FA_OPEN_ALWAYS | FA_WRITE);
        if (fr != FR_OK) return SD_ERR_OPEN;
    }

    /* 定位到文件末尾 */
    f_lseek(&fil, f_size(&fil));

    /* 写入数据 */
    UINT bw;
    fr = f_write(&fil, data, strlen(data), &bw);
    if (fr != FR_OK || bw != strlen(data)) {
        f_close(&fil);
        return SD_ERR_WRITE;
    }

    /* 同步确保写入 */
    f_sync(&fil);
    f_close(&fil);

    return SD_OK;
}

int SD_Card_IsInserted(void)
{
    /* PC0 = SD_CD, 低电平表示插入 */
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_RESET) ? 1 : 0;
}
