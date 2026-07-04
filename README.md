# STM32 Smart Environmental Monitoring Station (SEMS)

## 📋 项目概述

基于 STM32F407VGT6 的智能多传感器环境监测系统，集成温度、湿度、气压、空气质量、光照等多维环境感知，支持 OLED 本地显示、WiFi 远程数据上云、SD 卡数据存储及智能告警功能。采用 FreeRTOS 实时操作系统，模块化分层设计，具备完整的产品级嵌入式软件架构。

## 🎯 项目亮点（简历加分点）

| 能力维度 | 具体体现 |
|---------|---------|
| **RTOS 应用** | FreeRTOS 多任务调度、信号量/队列/软件定时器 |
| **外设驱动** | GPIO/ADC/I2C/SPI/UART/PWM/TIM 全覆盖 |
| **通信协议** | MQTT + ESP8266 AT 指令、I2C、SPI、单总线 |
| **传感器融合** | 多传感器数据采集、滤波、校准算法 |
| **低功耗设计** | STOP 模式、动态时钟调整、外设按需供电 |
| **数据结构** | 环形缓冲区、状态机、链表存储管理 |
| **硬件设计** | 原理图设计、信号完整性、电源管理 |
| **可维护性** | 分层架构、模块解耦、单元测试、Doxygen 注释 |

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────┐
│                    应用层                            │
│  SensorManager  DataLogger  WiFiManager  AlarmMgr   │
├─────────────────────────────────────────────────────┤
│                    服务层                            │
│    FreeRTOS (调度/同步)  │   CLI Debug Console      │
├─────────────────────────────────────────────────────┤
│                    驱动抽象层                         │
│   DHT22  BMP280  MQ135  SSD1306  ESP8266  SD卡     │
├─────────────────────────────────────────────────────┤
│                  HAL 硬件抽象层                       │
│         STM32F4xx HAL + CMSIS                        │
├─────────────────────────────────────────────────────┤
│                  硬件平台                             │
│         STM32F407VGT6 (STM32F4-Discovery)            │
└─────────────────────────────────────────────────────┘
```

## 🔧 硬件清单

| 器件 | 型号 | 接口 | 功能 |
|-----|------|------|------|
| MCU | STM32F407VGT6 | - | 主控，Cortex-M4 @168MHz |
| 温湿度 | DHT22 | 单总线 GPIO | -40~80°C, 0~100%RH |
| 气压 | BMP280 | I2C1 | 300~1100hPa |
| 空气质量 | MQ-135 | ADC1_IN0 | CO2/NH3/苯等 |
| 光照 | LDR + 分压 | ADC1_IN1 | 环境光强度 |
| 显示屏 | SSD1306 0.96" | I2C1 | 128×64 OLED |
| WiFi | ESP8266-01S | UART2 | AT指令 + MQTT |
| 存储 | MicroSD | SPI1 | FATFS 日志记录 |
| 蜂鸣器 | 有源蜂鸣器 | GPIO + TIM PWM | 告警输出 |
| LED | RGB LED | GPIO PWM | 状态指示 |

## 📁 项目结构

```
stm32-weather-station/
├── README.md                       # 项目说明（本文件）
├── LICENSE                         # 开源协议
│
├── Docs/
│   ├── hardware_connections.md     # 硬件接线文档（含原理图）
│   ├── architecture.md             # 软件架构详细说明
│   ├── api_reference.md            # API 接口文档
│   └── test_report.md              # 测试报告模板
│
├── Core/                           # 核心代码
│   ├── Inc/                        # 头文件
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h    # HAL 库配置
│   │   ├── stm32f4xx_it.h          # 中断声明
│   │   ├── FreeRTOSConfig.h        # RTOS 配置
│   │   ├── sensors/                # 传感器驱动头文件
│   │   ├── display/                # 显示驱动头文件
│   │   ├── comm/                   # 通信驱动头文件
│   │   ├── storage/                # 存储驱动头文件
│   │   └── app/                    # 应用层头文件
│   │
│   └── Src/                        # 源文件
│       ├── main.c                  # 主程序入口
│       ├── stm32f4xx_it.c          # 中断服务函数
│       ├── stm32f4xx_hal_msp.c     # HAL 底层初始化
│       ├── freertos.c              # FreeRTOS 任务创建
│       ├── sensors/                # 传感器驱动实现
│       ├── display/                # 显示驱动实现
│       ├── comm/                   # 通信驱动实现
│       ├── storage/                # 存储驱动实现
│       └── app/                    # 应用层实现
│
├── Drivers/                        # 官方驱动库
│   ├── STM32F4xx_HAL_Driver/       # STM32F4 HAL 驱动
│   └── CMSIS/                      # CMSIS 核心层
│
├── Middlewares/                     # 中间件
│   └── FreeRTOS/                   # FreeRTOS 源码
│
├── stm32_weather_station.ioc       # CubeMX 配置文件
├── .cproject                       # IDE 项目文件
├── .project                        # IDE 项目文件
├── Makefile                        # GCC 编译脚本
├── SConscript                       # 可选 SCons 构建
│
└── Tools/                          # 开发辅助工具
    ├── data_parser.py              # 日志解析 Python 脚本
    └── mqtt_dashboard.py           # 简易 MQTT 仪表盘
```

## 🚀 快速开始

### 环境要求
- STM32CubeIDE 1.13+ 或 Keil MDK v5
- STM32CubeMX 6.8+
- ARM GCC Toolchain (可选)
- Python 3.8+（用于 PC 端工具）

### 编译烧录
```bash
# 方法1：STM32CubeIDE 导入
File → Import → Existing Projects → 选择项目根目录

# 方法2：命令行编译（需 ARM GCC）
make -j4
make flash
```

### 硬件连接
详见 [Docs/hardware_connections.md](Docs/hardware_connections.md)

## 📊 功能特性

### 1. 传感器采集
- **DHT22**: 每 2s 采集温湿度，软件滤波 + 超时重试
- **BMP280**: 每 5s 采集气压/温度，I2C 连续模式
- **MQ-135**: 每 1s ADC 采样，自适应基线校准
- **LDR**: 每 1s ADC 采样, 指数平滑滤波

### 2. OLED 显示
- 主页面：实时数据轮播
- 历史页面：最近 24h 趋势图（像素级绘制）
- 设置页面：告警阈值调节
- 信息页面：WiFi 状态、IP、系统运行时间

### 3. WiFi + MQTT 上云
- ESP8266 AT 指令驱动，状态机管理
- MQTT 连接阿里云/华为云/公共 Broker
- JSON 数据上报，心跳保活
- 断线自动重连，数据缓冲

### 4. SD 卡数据存储
- FATFS 文件系统，CSV 格式记录
- 每 5 分钟自动归档
- 循环存储（最大 30 天）
- 插入检测 + 热插拔保护

### 5. 智能告警
- 温度/湿度/气压/空气质量阈值可配置
- 蜂鸣器 + 状态 LED 告警
- OLED 弹窗提示
- MQTT 远程告警推送

### 6. 低功耗模式
- STOP 模式下 200μA
- RTC 定时唤醒采集
- 外设按需供电控制

## 📈 RTOS 任务设计

| 任务名 | 优先级 | 周期 | 栈大小 | 功能 |
|-------|--------|------|--------|------|
| `Task_Sensor` | 3 | 1s | 512 | 传感器数据采集 |
| `Task_Display` | 2 | 500ms | 1024 | OLED 界面刷新 |
| `Task_WiFi` | 2 | 事件触发 | 1024 | WiFi/MQTT 通信 |
| `Task_Storage` | 1 | 事件触发 | 512 | SD 卡写入 |
| `Task_Alarm` | 3 | 事件触发 | 256 | 告警处理 |
| `Task_CLI` | 1 | 轮询 | 512 | 串口调试控制台 |

## 📝 代码规范

- 遵循 MISRA-C:2012 子集
- Doxygen 格式注释
- 匈牙利命名法 + 下划线分段
- 每个模块独立 .h/.c，单职责原则
- 静态代码分析: Cppcheck + PC-Lint

## 📄 License

MIT License
