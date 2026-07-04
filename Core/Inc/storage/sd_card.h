/**
 * @file    sd_card.h
 * @brief   MicroSD 卡驱动 (SPI 模式 + FATFS)
 *          支持文件读写、CSV 日志记录、插入检测
 */
#ifndef __SD_CARD_H
#define __SD_CARD_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SD_OK           0
#define SD_ERR_NOT_INIT 1
#define SD_ERR_MOUNT    2
#define SD_ERR_OPEN     3
#define SD_ERR_WRITE    4
#define SD_ERR_NO_CARD  5

/**
 * @brief  初始化 SD 卡 SPI 接口
 * @param  spi   SPI 句柄
 * @param  cs_pin SPI 片选引脚号
 */
void SD_Card_Init(SPI_HandleTypeDef *spi, uint16_t cs_pin);

/**
 * @brief  挂载 SD 卡文件系统
 * @return SD_OK 表示成功
 */
int SD_Card_Mount(void);

/**
 * @brief  卸载 SD 卡文件系统
 */
void SD_Card_Unmount(void);

/**
 * @brief  追加数据到 CSV 文件 (自动创建文件头)
 * @param  filename 文件名
 * @param  data     数据行
 * @return SD_OK 表示成功
 */
int SD_Card_AppendFile(const char *filename, const char *data);

/**
 * @brief  检查 SD 卡是否插入
 * @return 1=已插入, 0=未插入
 */
int SD_Card_IsInserted(void);

#endif
