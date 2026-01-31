# Makefile for FEMU test firmware

PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump

CFLAGS = -mcpu=cortex-m4 -mthumb -g -Os -Wall
ASFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -g
LDFLAGS = -T link.ld

# Test programs
TESTS = test_simple test_arithmetic test_memory test_branch \
        test_bitfield test_multiply test_stack test_it_block \
        test_compare test_extend test_load_store_misc test_move \
        test_shift_rotate test_saturate test_exclusive test_table_branch \
        test_adr test_system test_exception test_misc_alu \
        test_dsp test_pack test_fpu test_acquire_release test_sat_add \
        test_fpu_mac test_dsp_exchange test_multiply_halfword \
        test_system_hints test_fpu_loadstore

all: $(addsuffix .elf,$(TESTS)) $(addsuffix .bin,$(TESTS))

# ARMv8-M specific flags (for acquire/release tests)
ASFLAGS_V8M = -mcpu=cortex-m33 -mthumb -g

# Generic assembly rule
%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

# ARMv8-M specific tests
test_acquire_release.o: test_acquire_release.S
	$(AS) $(ASFLAGS_V8M) -o $@ $<

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
