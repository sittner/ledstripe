
TARGET = ledstripe

DEVICE = XMC4700
DEVICE_PKG = F144
DEVICE_SRAM = 2048
FCCU = 144000000

CPPFLAGS = -D$(DEVICE)_$(DEVICE_PKG)x$(DEVICE_SRAM) -DFCCU=$(FCCU)
CPPFLAGS += -I.

TARGET_SRC = \
	main.c \
	stripe.c \
	font.c \

XMC_LIB = ./XMC_Peripheral_Library_v2.1.24

VPATH += $(XMC_LIB)/CMSIS/Infineon/$(DEVICE)_series/Source
VPATH += $(XMC_LIB)/CMSIS/Infineon/$(DEVICE)_series/Source/GCC
CPPFLAGS += -I$(XMC_LIB)/CMSIS/Include
CPPFLAGS += -I$(XMC_LIB)/CMSIS/Infineon/$(DEVICE)_series/Include
LINKER_SCRIPT = $(XMC_LIB)/CMSIS/Infineon/$(DEVICE)_series/Source/GCC/$(DEVICE)x$(DEVICE_SRAM).ld
LIB_CMSIS_SRC = \
	system_$(DEVICE).c \
	startup_$(DEVICE).S \

VPATH += $(XMC_LIB)/ThirdPartyLibraries
LIB_NEWLIB_SRC = \
	Newlib/syscalls.c \

VPATH += $(XMC_LIB)/XMCLib/src
CPPFLAGS += -I$(XMC_LIB)/XMCLib/inc
LIB_XMCLIB_SRC = \
	xmc_dma.c \
	xmc_spi.c \
	xmc_usic.c \
	xmc_gpio.c \
	xmc_ccu4.c \
	xmc_eru.c \
	xmc4_scu.c \
	xmc4_gpio.c \

SRCS = \
	$(TARGET_SRC) \
	$(LIB_CMSIS_SRC) \
	$(LIB_NEWLIB_SRC) \
	$(LIB_XMCLIB_SRC) \

CSRCS = $(filter %.c,$(SRCS))
ASRCS = $(filter %.S,$(SRCS))

#LIBS = -lm

# JLink options
JLINK_SPEED = 1000
JLINK_IFACE = swd
JLINK_DEVICE = $(DEVICE)-$(DEVICE_SRAM)

CPPFLAGS += -MMD -MP -MF $(@:.o=.d) -MT $(@)
CPPFLAGS += -DARM_MATH_CM4


ARCHFLAGS = -mfloat-abi=softfp -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb

OBJFLAGS = -Wall -fmessage-length=0 -Wa,-adhlns=$(@:.o=.lst) $(ARCHFLAGS)

CFLAGS = $(CPPFLAGS) $(OBJFLAGS) -O3 -ffunction-sections -fdata-sections  -std=gnu99 -pipe
ASFLAGS = -x assembler-with-cpp $(CPPFLAGS) $(OBJFLAGS)

LDFLAGS = -T$(LINKER_SCRIPT) -nostartfiles -Xlinker --gc-sections
LDFLAGS += -specs=nano.specs -specs=nosys.specs
LDFLAGS += -Wl,-Map,$(TARGET).map
LDFLAGS += $(ARCHFLAGS)

# Define all object files.
OBJS = $(addprefix obj/, $(CSRCS:.c=.o) $(ASRCS:.S=.o))

# Define all listing files.
LSTS = $(addprefix obj/, $(CSRCS:.c=.lst) $(ASRCS:.S=.lst))

# Define all dependency files.
DEPS = $(addprefix obj/, $(CSRCS:.c=.d) $(ASRCS:.S=.d))

# Define programs and commands.
SHELL = sh
REMOVE = rm -f
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size
JLINK = JLinkExe

# Define Messages
# English
MSG_SIZE = Size: 
MSG_FLASH_FILE = Creating load file for Flash:
MSG_FLASH = Programming target flash memory:
MSG_FLASH_ERASE = Erasing target flash memory:
MSG_EXTENDED_LISTING = Creating Extended Listing:
MSG_LINKING = Linking:
MSG_COMPILING = Compiling:
MSG_ASSEMBLING = Assembling:
MSG_OBJCOPY = ObjCopy:
MSG_CLEANING = Cleaning project:

# Default target.
all: $(TARGET).hex size

# Compile: create object files from C source files.
obj/%.o: %.c
	@echo
	@echo $(MSG_COMPILING) $<
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

# Assemble: create object files from assembler source files.
obj/%.o: %.S
	@echo
	@echo $(MSG_ASSEMBLING) $<
	@mkdir -p $(dir $@)
	$(CC) -c $(ASFLAGS) -o $@ $<

# ObjCopy: create object files from binary files.
obj/%.o: %.bin
	@echo
	@echo $(MSG_OBJCOPY) $<
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm $< $@

# Link: create ELF output file from object files.
$(TARGET).elf: $(OBJS)
	@echo
	@echo $(MSG_LINKING) $@
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) 

# Create final output file from ELF output file.
$(TARGET).hex: $(TARGET).elf
	@echo
	@echo $(MSG_FLASH_FILE) $@
	$(OBJCOPY) -O ihex $< $@

# Create extended listing file from ELF output file.
$(TARGET).lss: $(TARGET).elf
	@echo
	@echo $(MSG_EXTENDED_LISTING) $@
	$(OBJDUMP) -h -S $(TARGET).elf > $(TARGET).lss

# Display size of file.
size: $(TARGET).elf
	@echo
	@echo $(MSG_SIZE)
	$(SIZE) --format=berkeley $(TARGET).elf
	@echo

# Flash the target device.
flash: $(TARGET).hex
	@echo
	@echo $(MSG_FLASH)
	echo \
'set timeout 120\n'\
'spawn "JLinkExe"\n'\
'expect "J-Link>" { send "device $(JLINK_DEVICE)\\n" }\n'\
'expect "J-Link>" { send "speed $(JLINK_SPEED)\\n" }\n'\
'expect "J-Link>" { send "si $(JLINK_IFACE)\\n" }\n'\
'expect "J-Link>" { send "loadfile $(TARGET).hex\\n" }\n'\
'expect "J-Link>" { send "r\\n" }\n'\
'expect "J-Link>" { send "g\\n" }\n'\
'expect "J-Link>" { send "qc\\n" }\n'\
	| expect
	@echo

# Erase the target flash
erase:
	@echo
	@echo $(MSG_FLASH_ERASE)
	echo \
'set timeout 120\n'\
'spawn "JLinkExe"\n'\
'expect "J-Link>" { send "device $(JLINK_DEVICE)\\n" }\n'\
'expect "J-Link>" { send "speed $(JLINK_SPEED)\\n" }\n'\
'expect "J-Link>" { send "si $(JLINK_IFACE)\\n" }\n'\
'expect "J-Link>" { send "erase\\n" }\n'\
'expect "J-Link>" { send "qc\\n" }\n'\
	| expect
	@echo

# Target: clean project.
clean:
	@echo
	@echo $(MSG_CLEANING)
	$(REMOVE) $(TARGET).map
	$(REMOVE) $(TARGET).elf
	$(REMOVE) $(TARGET).hex
	$(REMOVE) $(TARGET).lss
	$(REMOVE) -r obj

# Include the dependency files.
-include $(DEPS)

# Listing of phony targets.
.PHONY: all clean size flash erase

