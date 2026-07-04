/**
 * @file    data_logger.c
 * @brief   数据记录器
 *          使用 FatFs 将传感器数据写入 SD 卡 CSV 文件
 */
#include "app/data_logger.h"
#include "storage/sd_card.h"
#include <string.h>

static char log_buffer[LOGGER_BUF_SIZE];
static uint16_t buffer_pos = 0;
static uint32_t total_records = 0;
static uint32_t last_flush_time = 0;

/* CSV 文件头 */
static const char *csv_header = "timestamp,temperature,humidity,pressure,aqi,light\n";

int DataLogger_Init(void)
{
    buffer_pos = 0;
    total_records = 0;
    last_flush_time = HAL_GetTick();
    memset(log_buffer, 0, sizeof(log_buffer));
    return 0;
}

void DataLogger_LogSensorData(const SensorData_t *data)
{
    char line[128];
    int len = snprintf(line, sizeof(line),
        "%lu,%.1f,%.1f,%.1f,%.0f,%.0f\n",
        data->timestamp,
        (double)data->temperature,
        (double)data->humidity,
        (double)data->pressure,
        (double)data->air_quality_ppm,
        (double)data->light_lux);

    if (len > 0) {
        /* 写入环形缓冲区 */
        if (buffer_pos + len < LOGGER_BUF_SIZE) {
            memcpy(log_buffer + buffer_pos, line, len);
            buffer_pos += len;
            total_records++;
        } else {
            /* 缓冲区满, 先刷新 */
            DataLogger_Flush();
            memcpy(log_buffer, line, len);
            buffer_pos = len;
            total_records++;
        }
    }

    /* 定时自动刷新 */
    if (HAL_GetTick() - last_flush_time > LOGGER_FLUSH_INTERVAL) {
        DataLogger_Flush();
    }
}

void DataLogger_LogEvent(const char *event)
{
    char line[128];
    int len = snprintf(line, sizeof(line), "# EVENT [%lu]: %s\n",
                       HAL_GetTick(), event);
    if (len > 0 && buffer_pos + len < LOGGER_BUF_SIZE) {
        memcpy(log_buffer + buffer_pos, line, len);
        buffer_pos += len;
    }
}

void DataLogger_Flush(void)
{
    if (buffer_pos == 0) return;

    if (SD_Card_Mount() == SD_OK) {
        SD_Card_AppendFile("sems_log.csv", log_buffer);
        SD_Card_Unmount();
    }

    buffer_pos = 0;
    last_flush_time = HAL_GetTick();
}

void DataLogger_GetStats(uint32_t *records, uint32_t *disk_kb)
{
    *records = total_records;
    *disk_kb = (total_records * 64) / 1024;  /* 估算 */
}
