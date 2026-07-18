# Makefile for arduino-nrf54 -- builds one example sketch against the
# nRF54L15-DK target. This is the same recipe validated by hand during
# development (see docs/VERIFICATION.md) and is what CI runs.
#
# Usage:
#   make EXAMPLE=Blink
#   make EXAMPLE=SerialEcho
#
# Requires arm-none-eabi-gcc/g++/objcopy/size on PATH (or set TOOLCHAIN_PREFIX
# to a full path prefix, e.g. TOOLCHAIN_PREFIX=/path/to/bin/arm-none-eabi-).

EXAMPLE ?= Blink
BUILD_DIR := build/$(EXAMPLE)
TOOLCHAIN_PREFIX ?= arm-none-eabi-

CC      := $(TOOLCHAIN_PREFIX)gcc
CXX     := $(TOOLCHAIN_PREFIX)g++
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SIZE    := $(TOOLCHAIN_PREFIX)size

NRFX := extern/nrfx
MDK  := $(NRFX)/bsp/stable/mdk/nrf54l/nrf54l15
MDK_COMMON := $(NRFX)/bsp/stable/mdk/common
CMSIS := extern/CMSIS_6/CMSIS/Core/Include

INCLUDES := \
  -Icores/nrf54l -Ivariants/nrf54l15dk \
  -I$(NRFX) -I$(NRFX)/drivers/include -I$(NRFX)/hal -I$(NRFX)/haly \
  -I$(NRFX)/lib -I$(NRFX)/helpers \
  -I$(NRFX)/bsp/stable -I$(NRFX)/bsp/stable/mdk \
  -I$(MDK) -I$(MDK_COMMON) \
  -I$(CMSIS)

DEFINES := -DNRF54L15_XXAA -DNRF_APPLICATION -D__STARTUP_CLEAR_BSS

MCU_FLAGS := -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16

CFLAGS   := $(MCU_FLAGS) -std=gnu11   -Wall -O1 $(DEFINES) $(INCLUDES)
CXXFLAGS := $(MCU_FLAGS) -std=gnu++17 -Wall -O1 -fno-exceptions -fno-rtti $(DEFINES) $(INCLUDES)
ASFLAGS  := $(MCU_FLAGS) -x assembler-with-cpp $(DEFINES) $(INCLUDES)

LDSCRIPT := $(MDK)/nrf54l15_xxaa_application.ld
LDFLAGS  := $(MCU_FLAGS) -T$(LDSCRIPT) -L$(MDK) -L$(MDK_COMMON) \
            -specs=nano.specs -specs=nosys.specs \
            -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(EXAMPLE).map

CORE_C_SRCS := \
  cores/nrf54l/nrfx_glue.c \
  cores/nrf54l/wiring_digital.c \
  cores/nrf54l/wiring_time.c \
  cores/nrf54l/wiring_analog.c \
  cores/nrf54l/wiring_interrupts.c \
  cores/nrf54l/syscalls.c

CORE_CXX_SRCS := \
  cores/nrf54l/Print.cpp \
  cores/nrf54l/HardwareSerial.cpp \
  cores/nrf54l/SPI.cpp \
  cores/nrf54l/Wire.cpp \
  cores/nrf54l/main.cpp

NRFX_C_SRCS := \
  $(NRFX)/drivers/src/nrfx_uarte.c \
  $(NRFX)/drivers/src/nrfx_grtc.c \
  $(NRFX)/drivers/src/nrfx_spim.c \
  $(NRFX)/drivers/src/nrfx_twim.c \
  $(NRFX)/drivers/src/nrfx_saadc.c \
  $(NRFX)/drivers/src/nrfx_pwm.c \
  $(NRFX)/drivers/src/nrfx_gpiote.c \
  $(NRFX)/helpers/nrfx_flag32_allocator.c

MDK_C_SRCS := $(MDK)/../system_nrf54l.c
STARTUP_ASM := $(MDK)/gcc_startup_nrf54l15_application.S

SKETCH := examples/$(EXAMPLE)/$(EXAMPLE).ino

C_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(CORE_C_SRCS) $(NRFX_C_SRCS) $(MDK_C_SRCS)))
CXX_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(CORE_CXX_SRCS)))
ASM_OBJS := $(BUILD_DIR)/startup.o
SKETCH_OBJ := $(BUILD_DIR)/$(EXAMPLE).o

ALL_OBJS := $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS) $(SKETCH_OBJ)

vpath %.c cores/nrf54l:$(NRFX)/drivers/src:$(NRFX)/helpers:$(MDK)/..
vpath %.cpp cores/nrf54l

.PHONY: all clean
all: $(BUILD_DIR)/$(EXAMPLE).elf $(BUILD_DIR)/$(EXAMPLE).hex
	$(SIZE) $(BUILD_DIR)/$(EXAMPLE).elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SKETCH_OBJ): $(SKETCH) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -x c++ -c $< -o $@

$(ASM_OBJS): $(STARTUP_ASM) | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(EXAMPLE).elf: $(ALL_OBJS)
	$(CC) $(LDFLAGS) $(ALL_OBJS) -o $@

$(BUILD_DIR)/$(EXAMPLE).hex: $(BUILD_DIR)/$(EXAMPLE).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf build
