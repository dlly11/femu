# Makefile for FEMU test firmware

PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump

# Flags for assembly-only tests
ASFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -g
LDFLAGS = -T link.ld

# Flags for C tests (with FPU, freestanding)
ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS = $(ARCH_FLAGS) -g -Os -Wall -ffreestanding -fno-builtin -nostdlib -Ic

# Assembly-only test programs
TESTS = test_simple test_arithmetic test_memory test_branch \
        test_bitfield test_multiply test_stack test_it_block \
        test_compare test_extend test_load_store_misc test_move \
        test_shift_rotate test_saturate test_exclusive test_table_branch \
        test_adr test_system test_exception test_misc_alu \
        test_dsp test_pack test_fpu test_acquire_release test_sat_add \
        test_fpu_mac test_dsp_exchange test_multiply_halfword \
        test_system_hints test_fpu_loadstore \
        test_mpu_basic test_nvic_basic test_peripheral test_irq

# TrustZone tests (require cortex-m33 and special linker script)
TZ_TESTS = test_tt_insn test_sg_insn test_bxns_blxns

# C test programs
C_TESTS = test_c_arithmetic test_c_memory test_c_bitfield test_c_branch \
          test_c_fpu test_c_dsp test_c_system test_c_exception

all: $(addsuffix .elf,$(TESTS)) $(addsuffix .bin,$(TESTS)) \
     $(addsuffix .elf,$(C_TESTS)) $(addsuffix .bin,$(C_TESTS)) \
     $(addsuffix .elf,$(TZ_TESTS)) $(addsuffix .bin,$(TZ_TESTS))

# ARMv8-M specific flags (for acquire/release and TrustZone tests)
ASFLAGS_V8M = -mcpu=cortex-m33 -mthumb -g
LDFLAGS_TZ = -T link_trustzone.ld

# Generic assembly rule
%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

# ARMv8-M specific tests (acquire/release)
test_acquire_release.o: test_acquire_release.S
	$(AS) $(ASFLAGS_V8M) -o $@ $<

# TrustZone tests need cortex-m33 assembler flags
test_tt_insn.o: test_tt_insn.S
	$(AS) $(ASFLAGS_V8M) -o $@ $<

test_sg_insn.o: test_sg_insn.S
	$(AS) $(ASFLAGS_V8M) -o $@ $<

test_bxns_blxns.o: test_bxns_blxns.S
	$(AS) $(ASFLAGS_V8M) -o $@ $<

# Generic link rule
%.elf: %.o link.ld
	$(LD) $(LDFLAGS) -o $@ $<
	$(OBJDUMP) -d $@ > $*.lst

# TrustZone tests use special linker script
test_tt_insn.elf: test_tt_insn.o link_trustzone.ld
	$(LD) $(LDFLAGS_TZ) -o $@ $<
	$(OBJDUMP) -d $@ > test_tt_insn.lst

test_sg_insn.elf: test_sg_insn.o link_trustzone.ld
	$(LD) $(LDFLAGS_TZ) -o $@ $<
	$(OBJDUMP) -d $@ > test_sg_insn.lst

test_bxns_blxns.elf: test_bxns_blxns.o link_trustzone.ld
	$(LD) $(LDFLAGS_TZ) -o $@ $<
	$(OBJDUMP) -d $@ > test_bxns_blxns.lst

# Generic rules
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

# C runtime startup
c/startup.o: c/startup.S
	$(CC) $(ARCH_FLAGS) -c -o $@ $<

# C test source files
c/%.o: c/%.c c/test_framework.h c/armv8m.h
	$(CC) $(CFLAGS) -c -o $@ $<

# C test linking (startup + test)
$(C_TESTS:%=%.elf): %.elf: c/startup.o c/%.o link.ld
	$(LD) $(LDFLAGS) -o $@ c/startup.o c/$*.o
	$(OBJDUMP) -d $@ > $*.lst

clean:
	rm -f *.o *.elf *.bin *.lst c/*.o

.PHONY: all clean
