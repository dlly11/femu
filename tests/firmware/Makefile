# Makefile for FEMU test firmware

PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump

CFLAGS = -mcpu=cortex-m4 -mthumb -g -Os -Wall
ASFLAGS = -mcpu=cortex-m4 -mthumb -g
LDFLAGS = -T link.ld

# Test programs
TESTS = test_simple test_arithmetic test_memory test_branch \
        test_bitfield test_multiply test_stack test_it_block

all: $(addsuffix .elf,$(TESTS)) $(addsuffix .bin,$(TESTS))

# Generic assembly rule
%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

# Generic link rule
%.elf: %.o link.ld
	$(LD) $(LDFLAGS) -o $@ $<
	$(OBJDUMP) -d $@ > $*.lst

# Generic rules
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f *.o *.elf *.bin *.lst

.PHONY: all clean
