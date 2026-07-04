/**
 * @file    data_logger.h
 * @brief   数据日志模块
 *          管理 CSV 格式数据记录、文件轮转、缓冲写入
 */
#ifndef __DATA_LOGGER_H
#define __DATA_LOGGER_H

#include "main.h"

/* 日志配置 */
#define LOGGER_BUF_SIZE      512    /* 环形缓冲区大小 */
#define LOGGER_FLUSH_INTERVAL 60000 /* 自动刷新间隔 (ms) */
#define LOG_MAX_DAYS         30     /* 最大保留天数 */

int  DataLogger_Init(void);
void DataLogger_LogSensorData(const SensorData_t *data);
void DataLogger_LogEvent(const char *event);
void DataLogger_Flush(void);
void DataLogger_GetStats(uint32_t *total_records, uint32_t *disk_used_kb);

#endif
