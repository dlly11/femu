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
        test_bitfield test_multiply test_stack test_it_block \
        test_compare test_extend test_load_store_misc test_move \
        test_shift_rotate test_saturate test_exclusive test_table_branch \
        test_adr test_system test_exception test_misc_alu

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
