# 硬件连接文档

## STM32F407VGT6 引脚分配表

| 功能 | 外设 | 引脚 | 复用功能 | 备注 |
|------|------|------|---------|------|
| **I2C1_SCL** | BMP280 / SSD1306 | PB6 | AF4 | 共用 I2C1 总线 |
| **I2C1_SDA** | BMP280 / SSD1306 | PB7 | AF4 | 共用 I2C1 总线 |
| **USART1_TX** | 调试串口 (CP2102) | PA9 | AF7 | 115200-8N1 |
| **USART1_RX** | 调试串口 (CP2102) | PA10 | AF7 | 115200-8N1 |
| **USART2_TX** | ESP8266-01S | PA2 | AF7 | 115200-8N1 |
| **USART2_RX** | ESP8266-01S | PA3 | AF7 | 115200-8N1 |
| **SPI1_NSS** | MicroSD 卡 | PA4 | AF5 | SPI 片选 |
| **SPI1_SCK** | MicroSD 卡 | PA5 | AF5 | SPI 时钟 |
| **SPI1_MISO** | MicroSD 卡 | PA6 | AF5 | SPI 主入从出 |
| **SPI1_MOSI** | MicroSD 卡 | PA7 | AF5 | SPI 主出从入 |
| **DHT22_DATA** | DHT22 | PB0 | GPIO_OUT | 单总线，OD 开漏 |
| **ADC1_IN0** | MQ-135 | PA0 | Analog | 空气质量 |
| **ADC1_IN1** | LDR 光敏 | PA1 | Analog | 光照强度 |
| **BUZZER_PWM** | 有源蜂鸣器 | PB1 | AF2(TIM3_CH4) | PWM 频率控制 |
| **RGB_RED** | RGB LED | PE0 | TIM4_CH1 | PWM 调光 |
| **RGB_GREEN** | RGB LED | PE1 | TIM4_CH2 | PWM 调光 |
| **RGB_BLUE** | RGB LED | PE2 | TIM4_CH3 | PWM 调光 |
| **SD_CD** | SD 卡检测 | PC0 | GPIO_IN | 插入检测(低有效) |
| **ESP_RST** | ESP8266 复位 | PC1 | GPIO_OUT | 低电平复位 |
| **ESP_GPIO0** | ESP8266 模式 | PC2 | GPIO_OUT | 低=烧录模式 |

## 接线原理图

```
┌──────────────────────────────────────────────────┐
│                   STM32F407VGT6                    │
│                                                    │
│  PB6(SCL) ───┬── BMP280 SCL                       │
│               └── SSD1306 SCL                      │
│  PB7(SDA) ───┬── BMP280 SDA                       │
│               └── SSD1306 SDA                      │
│                                                    │
│  PA9(TX1) ─── CP2102 RXD (调试串口)                │
│  PA10(RX1) ── CP2102 TXD                          │
│                                                    │
│  PA2(TX2) ─── ESP8266 RXD                         │
│  PA3(RX2) ─── ESP8266 TXD                         │
│  PC1 ──────── ESP8266 RST                         │
│  PC2 ──────── ESP8266 GPIO0                       │
│                                                    │
│  PA5(SCK) ─── SD卡 CLK                            │
│  PA6(MISO) ── SD卡 DO                             │
│  PA7(MOSI) ── SD卡 DI                             │
│  PA4(CS) ──── SD卡 CS                             │
│  PC0 ──────── SD卡 CD (插入检测)                   │
│                                                    │
│  PB0 ──────── DHT22 DATA (4.7kΩ 上拉到 3.3V)      │
│                                                    │
│  PA0 ──────── MQ-135 AO (模拟输出)                 │
│  PA1 ──────── LDR + 10kΩ 分压 (模拟输入)           │
│                                                    │
│  PB1 ──────── 蜂鸣器 (NPN 三极管驱动)              │
│  PE0 ──────── RGB LED R (220Ω 限流)               │
│  PE1 ──────── RGB LED G (220Ω 限流)               │
│  PE2 ──────── RGB LED B (220Ω 限流)               │
└──────────────────────────────────────────────────┘
```

## 电源树

```
USB 5V
 ├── AMS1117-3.3 ─── 3.3V ─── STM32, DHT22, BMP280
 │                            SSD1306, SD Card
 │                            └── MIC5205-3.3 ─── ESP8266 (独立 LDO)
 │
 └── 通过 NPN 开关 ─── 5V ─── MQ-135 加热丝 (受控)
```

## 注意事项

1. **DHT22 上拉电阻**: DATA 线必须外接 4.7kΩ 上拉到 3.3V
2. **ESP8266 供电**: 峰值电流 300mA，独立 LDO 供电，加 100μF + 0.1μF 滤波电容
3. **SD 卡**: 使用 SPI 模式，SCK 不超过 20MHz
4. **I2C 上拉**: SCL/SDA 各加 4.7kΩ 上拉到 3.3V
5. **MQ-135 预热**: 上电后需预热 60s 读数才稳定
6. **电平匹配**: ESP8266 为 3.3V 电平，与 STM32 直连
