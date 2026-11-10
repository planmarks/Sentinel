# Muru Vault - STM32L442KCU6 (Sentinel V1B) firmware
# Build:  make        -> build/muru_vault.hex + .elf + .bin
#         make clean
# Flash:  STM32CubeProgrammer via ST-LINK V2 -> build/muru_vault.hex @ 0x08000000

TARGET  = muru_vault
BUILD   = build

# --- toolchain (STM32CubeIDE's bundled GNU arm toolchain is on PATH) ----------
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# --- sources ------------------------------------------------------------------
C_SRC   = $(wildcard src/*.c) $(wildcard src/crypto/*.c)
ASM_SRC = src/startup_stm32l442.s
LDSCRIPT = linker/stm32l442.ld

INCLUDES = -Isrc -Isrc/cmsis -Isrc/crypto

# --- flags --------------------------------------------------------------------
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS = -DSTM32L442xx -DVECT_TAB_ADDR=0x08008000U

CFLAGS  = $(MCU) $(DEFS) $(INCLUDES) -Os -g3 -std=c11 \
          -ffunction-sections -fdata-sections -fno-common \
          -Wall -Wextra -Wno-unused-parameter
ASFLAGS = $(MCU) -g3
LDFLAGS = $(MCU) -T$(LDSCRIPT) -specs=nano.specs -specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
          -Wl,--print-memory-usage -lc -lm

# --- objects ------------------------------------------------------------------
OBJ  = $(addprefix $(BUILD)/, $(notdir $(C_SRC:.c=.o)))
OBJ += $(addprefix $(BUILD)/, $(notdir $(ASM_SRC:.s=.o)))
vpath %.c src src/crypto
vpath %.s src

.PHONY: all clean size
all: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin size

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(CC) -c $(ASFLAGS) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJ) $(LDSCRIPT)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary -S $< $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.elf $(BUILD)/*.hex $(BUILD)/*.bin $(BUILD)/*.map
