# Muru Vault secure bootloader build.
#   make        -> build/bootloader.hex + .elf + .bin
# Flashed at 0x08000000. Verifies the app at 0x08008000 before jumping.

TARGET  = bootloader
BUILD   = build
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# bootloader-specific sources
BL_SRC   = main.c boot_verify.c boot_pubkey.c randombytes.c update.c bl_flash.c crypto/tweetnacl.c
# low-level drivers reused from the app
APP_SRC  = ../src/clock.c ../src/i2c.c ../src/ssd1306.c ../src/font.c ../src/rng.c \
           ../src/crypto/sha256.c ../src/system_stm32l4xx.c
ASM_SRC  = ../src/startup_stm32l442.s
LDSCRIPT = ../linker/bootloader.ld

INCLUDES = -I../src -I../src/cmsis -I../src/crypto -I. -Icrypto
MCU  = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS = -DSTM32L442xx

CFLAGS  = $(MCU) $(DEFS) $(INCLUDES) -Os -g3 -std=c11 \
          -ffunction-sections -fdata-sections -fno-common \
          -Wall -Wextra -Wno-unused-parameter
ASFLAGS = $(MCU) -g3
LDFLAGS = $(MCU) -T$(LDSCRIPT) -specs=nano.specs -specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

C_SRC = $(BL_SRC) $(APP_SRC)
OBJ  = $(addprefix $(BUILD)/, $(notdir $(C_SRC:.c=.o)))
OBJ += $(addprefix $(BUILD)/, $(notdir $(ASM_SRC:.s=.o)))
vpath %.c . crypto ../src ../src/crypto
vpath %.s ../src

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
