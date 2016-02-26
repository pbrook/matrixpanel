# Expects an existing build of the mbed libraries
# git submodule update --init ; cd mbed ; workspace_tools/build.py -m K64F -t GCC_ARM -u

CROSS = arm-none-eabi-
PROJECT = panel
OBJECTS = main.o

TT = TARGET_K64F/TOOLCHAIN_GCC_ARM

MBED = mbed/build/mbed
MBED_LIBDIR = $(MBED)/$(TT)
USB = mbed/build/usb
RTOS = mbed/build/rtos
ETH = mbed/build/net/eth

SYS_OBJECTS = $(MBED_LIBDIR)/mbed_overrides.o $(MBED_LIBDIR)/system_MK64F12.o $(MBED_LIBDIR)/startup_MK64F12.o $(MBED_LIBDIR)/board.o $(MBED_LIBDIR)/cmsis_nvic.o $(MBED_LIBDIR)/retarget.o 

USB_INCLUDES = -I$(USB)/USBAudio -I$(USB)/USBDevice -I$(USB)/USBHID -I$(USB)/USBMIDI -I$(USB)/USBSerial -I$(USB)/USBMSD 
MBED_TARGET_DIR=$(MBED)/TARGET_K64F/TARGET_Freescale/TARGET_KPSDK_MCUS
MBED_INCLUDES = -I$(MBED) -I$(MBED)/TARGET_K64F \
	-I$(MBED)/TARGET_K64F/TARGET_Freescale \
	-I$(MBED_TARGET_DIR) \
	-I$(MBED_TARGET_DIR)/TARGET_MCU_K64F \
	-I$(MBED_TARGET_DIR)/TARGET_MCU_K64F/TARGET_FRDM \
	-I$(MBED_TARGET_DIR)/TARGET_MCU_K64F/device \
	-I$(MBED_TARGET_DIR)/TARGET_MCU_K64F/device/device \
	-I$(MBED_TARGET_DIR)/TARGET_MCU_K64F/device/MK64F12 \
	-I$(MBED_TARGET_DIR)/TARGET_KPSDK_CODE/hal/gpio

RTOS_INCLUDES = \
	-I$(RTOS) \
	-I$(RTOS)/TARGET_CORTEX_M

ETH_INCLUDES = \
	-I$(ETH)/EthernetInterface \
	-I$(ETH)/lwip/include \
	-I$(ETH)/lwip/include/ipv4 \
	-I$(ETH)/lwip \
	-I$(ETH)/lwip-sys \
	-I$(ETH)/lwip-eth/arch/TARGET_Freescale \
	-I$(ETH) \
	-I$(ETH)/Socket

INCLUDE_PATHS = $(MBED_INCLUDES) $(USB_INCLUDES) $(RTOS_INCLUDES) $(ETH_INCLUDES)
LIBRARY_PATHS = -L$(MBED_LIBDIR) -L$(USB)/$(TT) -L$(RTOS)/$(TT) -L$(ETH)/$(TT)
LIBRARIES = -leth -lrtos -lrtx -lmbed
LINKER_SCRIPT = $(MBED_LIBDIR)/K64FN1M0xxx12.ld

############################################################################### 
AS       = $(CROSS)as
CC       = $(CROSS)gcc
CPP      = $(CROSS)g++
LD       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
SIZE     = $(CROSS)size 

ifeq ($(HARDFP),1)
	FLOAT_ABI = hard
else
	FLOAT_ABI = softfp
endif


CPU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=$(FLOAT_ABI) 
CC_FLAGS = $(CPU) -c -g -fno-common -fmessage-length=0 -Wall -fno-exceptions -ffunction-sections -fdata-sections -fomit-frame-pointer -MMD -MP -fshort-wchar
CC_SYMBOLS = -DTARGET_KPSDK_MCUS -DTARGET_FF_ARDUINO -DTOOLCHAIN_GCC_ARM -DTOOLCHAIN_GCC -DCPU_MK64FN1M0VMD12 -DTARGET_FRDM -DTARGET_CORTEX_M -D__FPU_PRESENT=1 -DTARGET_KPSDK_CODE -DTARGET_M4 -D__MBED__=1 -DTARGET_K64F -DTARGET_Freescale -D__CORTEX_M4 -DFSL_RTOS_MBED -DTARGET_MCU_K64F -DARM_MATH_CM4 

LD_FLAGS = $(CPU) -Wl,--gc-sections --specs=nano.specs -u _printf_float -u _scanf_float -Wl,--wrap,main -Wl,-Map=$(PROJECT).map,--cref
LD_SYS_LIBS = -lstdc++ -lsupc++ -lm -lc -lgcc -lnosys


ifeq ($(DEBUG), 1)
  CC_FLAGS += -DDEBUG -O0
else
  CC_FLAGS += -DNDEBUG -Os
endif

.PHONY: all clean lst size

all: $(PROJECT).bin $(PROJECT).hex size


clean:
	rm -f $(PROJECT).bin $(PROJECT).elf $(PROJECT).hex $(PROJECT).map $(PROJECT).lst $(OBJECTS) $(DEPS)


.asm.o:
	$(CC) $(CPU) -c -x assembler-with-cpp -o $@ $<
.s.o:
	$(CC) $(CPU) -c -x assembler-with-cpp -o $@ $<
.S.o:
	$(CC) $(CPU) -c -x assembler-with-cpp -o $@ $<

.c.o:
	$(CC)  $(CC_FLAGS) $(CC_SYMBOLS) -std=gnu99   $(INCLUDE_PATHS) -o $@ $<

.cpp.o:
	$(CPP) $(CC_FLAGS) $(CC_SYMBOLS) -std=gnu++98 -fno-rtti $(INCLUDE_PATHS) -o $@ $<



$(PROJECT).elf: $(OBJECTS) $(SYS_OBJECTS)
	$(LD) $(LD_FLAGS) -T$(LINKER_SCRIPT) $(LIBRARY_PATHS) -o $@ $^ $(LIBRARIES) $(LD_SYS_LIBS) $(LIBRARIES) $(LD_SYS_LIBS)


$(PROJECT).bin: $(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(PROJECT).hex: $(PROJECT).elf
	@$(OBJCOPY) -O ihex $< $@

$(PROJECT).lst: $(PROJECT).elf
	@$(OBJDUMP) -Sdh $< > $@

lst: $(PROJECT).lst

size: $(PROJECT).elf
	$(SIZE) $(PROJECT).elf

DEPS = $(OBJECTS:.o=.d)
-include $(DEPS)

install: $(PROJECT).bin
	cp $(PROJECT).bin /mnt/mbed

