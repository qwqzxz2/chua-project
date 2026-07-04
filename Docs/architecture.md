# 软件架构设计文档

## 1. 设计原则

- **分层架构**: 硬件驱动 → 硬件抽象 → 服务层 → 应用层
- **模块化**: 每个模块单一职责，接口清晰
- **可移植性**: HAL 层隔离硬件差异
- **实时性**: FreeRTOS 保证时序关键任务
- **可测试性**: 接口可 mock，支持单元测试

## 2. 分层详细设计

### 2.1 HAL 层 (Hardware Abstraction Layer)
- ST 官方 STM32F4xx_HAL_Driver
- 封装 MCU 外设寄存器操作
- 提供统一 API：HAL_XXX_Init/DeInit/Transmit/Receive

### 2.2 驱动层 (Driver Layer)
每个外设驱动封装为一个独立模块，提供：

```
┌───────────────────────────────────────────────────┐
│                  Driver Interface                   │
├───────────────────────────────────────────────────┤
│  int  XXX_Init(XXX_Handle_t* h)                   │
│  int  XXX_DeInit(XXX_Handle_t* h)                 │
│  int  XXX_Read(XXX_Handle_t* h, void* data)      │
│  int  XXX_Write(XXX_Handle_t* h, void* data)     │
│  void XXX_ErrorCallback(XXX_Handle_t* h, int err) │
└───────────────────────────────────────────────────┘
```

### 2.3 服务层 (Service Layer)
- `SensorManager`: 传感器轮询调度、数据融合、滤波
- `DataLogger`: 数据格式化、CSV 写入、文件管理
- `WiFiManager`: WiFi 连接状态机、MQTT 协议栈
- `AlarmManager`: 阈值比较、告警优先级、通知分发
- `DisplayManager`: 界面状态机、页面切换、动画

### 2.4 应用层 (Application Layer)
- `main.c`: 系统初始化、任务创建、启动调度器
- `freertos.c`: 任务函数实现、队列/信号量定义
- `app_config.h`: 全局配置参数

## 3. FreeRTOS 任务通信

### 3.1 队列定义

```
Queue_SensorData:   采集任务 → 显示任务 / 存储任务 / WiFi任务
    ├── 数据帧: {timestamp, temperature, humidity, pressure, air_quality, light}
    ├── 长度: 10
    └── 类型: SensorData_t

Queue_AlarmEvent:   告警任务 ← 传感器管理
    ├── 数据帧: {type, severity, value, threshold}
    ├── 长度: 5
    └── 类型: AlarmEvent_t

Queue_WiFiCmd:      应用层 → WiFi 任务
    ├── 数据帧: {cmd_id, payload, timeout_ms}
    ├── 长度: 5
    └── 类型: WiFiCmd_t
```

### 3.2 信号量

```
Sem_USART1_TX:      串口发送互斥
Sem_USART2_TX:      ESP8266 发送互斥
Sem_SPI1:           SD 卡 SPI 总线互斥
Sem_I2C1:           I2C 总线互斥
Sem_SD_Insertion:   SD 卡插入事件 (二进制)
Sem_WiFi_Connected: WiFi 连接状态 (二进制)
```

### 3.3 任务流

```
                    ┌──────────┐
                    │ Task_CLI │ (调试串口)
                    └────┬─────┘
                         │
┌───────────┐     ┌──────┴───────┐     ┌───────────┐
│ Task_Sensor│────>│ SensorManager│────>│Task_Display│
└───────────┘     └──────┬───────┘     └───────────┘
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
       ┌──────────┐ ┌────────┐ ┌─────────┐
       │Task_WiFi │ │Storage │ │ Alarms  │
       │ (MQTT)   │ │ (SD)   │ │ (Buzzer)│
       └──────────┘ └────────┘ └─────────┘
```

## 4. 传感器滤波算法

### 4.1 滑动平均滤波 (DHT22/BMP280)
```
buffer[N] → 移除最旧 → 插入最新 → 计算算术平均
N = 5, 有效去除高频噪声
```

### 4.2 指数加权滤波 (LDR 光照)
```
filtered = α × raw + (1 - α) × prev_filtered
α = 0.3, 对快速变化响应灵敏
```

### 4.3 自适应基线校准 (MQ-135)
```
baseline = median(last_24h_data)
calibrated_ppm = (current - baseline) / sensitivity + 400
```

## 5. 错误处理策略

| 错误类型 | 处理策略 |
|---------|---------|
| 传感器超时 | 重试 3 次，失败则标记为 NAN，触发告警 |
| WiFi 断连 | 指数退避重连 (1s→2s→4s→...→60s) |
| SD 卡写入失败 | 内存缓冲区暂存，恢复后补写 |
| I2C 总线锁死 | 硬件复位 I2C 外设，重新初始化 |
| 内存不足 | 裁剪数据精度，降低采样率 |
