# STM32F407VGT6 GCC Makefile
# Toolchain: arm-none-eabi-
# Usage: make all    (compile)
#        make flash  (flash via ST-Link)
#        make clean  (clean build)

PROJECT = sems-weather-station
TARGET  = $(PROJECT)

# CPU configuration
CPU = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
FPU = -D__FPU_USED=1 -DARM_MATH_CM4

# Directories
SRC_DIR  = Core/Src
INC_DIR  = Core/Inc
DRV_DIR  = Drivers
MID_DIR  = Middlewares
BUILD_DIR = Build

# Source files (Core)
SRC_CORE = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/stm32f4xx_it.c \
    $(SRC_DIR)/stm32f4xx_hal_msp.c \
    $(SRC_DIR)/sensors/dht22.c \
    $(SRC_DIR)/sensors/bmp280.c \
    $(SRC_DIR)/sensors/mq135.c \
    $(SRC_DIR)/display/ssd1306.c \
    $(SRC_DIR)/comm/esp8266.c \
    $(SRC_DIR)/comm/mqtt.c \
    $(SRC_DIR)/storage/sd_card.c \
    $(SRC_DIR)/app/sensor_manager.c \
    $(SRC_DIR)/app/data_logger.c \
    $(SRC_DIR)/app/wifi_manager.c \
    $(SRC_DIR)/app/alarm_manager.c

# HAL Driver source (select only needed modules)
SRC_HAL = \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
    $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c

# FreeRTOS source
SRC_FREERTOS = \
    $(MID_DIR)/FreeRTOS/tasks.c \
    $(MID_DIR)/FreeRTOS/queue.c \
    $(MID_DIR)/FreeRTOS/list.c \
    $(MID_DIR)/FreeRTOS/timers.c \
    $(MID_DIR)/FreeRTOS/event_groups.c \
    $(MID_DIR)/FreeRTOS/portable/GCC/ARM_CM4F/port.c \
    $(MID_DIR)/FreeRTOS/portable/MemMang/heap_4.c

# All sources
SRCS = $(SRC_CORE) $(SRC_HAL) $(SRC_FREERTOS)
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# Include paths
INCLUDES = \
    -I$(INC_DIR) \
    -I$(INC_DIR)/sensors \
    -I$(INC_DIR)/display \
    -I$(INC_DIR)/comm \
    -I$(INC_DIR)/storage \
    -I$(INC_DIR)/app \
    -I$(DRV_DIR)/CMSIS/Include \
    -I$(DRV_DIR)/CMSIS/Device/ST/STM32F4xx/Include \
    -I$(DRV_DIR)/STM32F4xx_HAL_Driver/Inc \
    -I$(MID_DIR)/FreeRTOS \
    -I$(MID_DIR)/FreeRTOS/include

# Compiler flags
CFLAGS  = $(CPU) $(FPU) -O2 -Wall -Wextra -Werror -Wshadow
CFLAGS += -std=c99 -Wno-strict-aliasing
CFLAGS += -DUSE_HAL_DRIVER -DSTM32F407xx
CFLAGS += $(INCLUDES)
CFLAGS += -ffunction-sections -fdata-sections

# Linker flags
LDFLAGS  = $(CPU) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map
LDFLAGS += -Wl,--start-group -lc -lm -lgcc -Wl,--end-group
LDFLAGS += -specs=nano.specs -specs=nosys.specs

# Linker script (ST's stock for STM32F407VG)
LDSCRIPT = stm32f407vg_flash.ld

# Default target
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex

# Compile
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -T$(LDSCRIPT) -o $@
	@echo "Build complete: $@"

# Binary output
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# Flash via ST-Link
flash: $(BUILD_DIR)/$(TARGET).bin
	@echo "Flashing via ST-Link..."
	st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x8000000

# Clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Dependencies
-include $(OBJS:.o=.d)

# Toolchain
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size
GDB     = arm-none-eabi-gdb

.PHONY: all clean flash
