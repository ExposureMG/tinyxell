# Builds tinyxell's stage1 as a raw binary for the hypervisor to copy to 0x1C000000, and as a C header for the
# launcher. Uses the libxenon toolchain (xenon-gcc), as xell-reloaded does, and its lv1 sources from the submodule.
CROSS ?= xenon-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy

LV1 = ../external/xell-reloaded/source/lv1
BUILD = build

CFLAGS = -Wall -Werror -Os -I../include -I$(LV1) -ffunction-sections -fdata-sections \
	-m64 -mno-toc -DBYTE_ORDER=BIG_ENDIAN -mno-altivec -ffreestanding -fno-builtin \
	-fno-tree-loop-distribute-patterns
AFLAGS = -I$(LV1) -m64
LDFLAGS = -nostdlib -n -m64 -Wl,--gc-sections

OBJS = $(BUILD)/startup.o $(BUILD)/main.o $(BUILD)/cache.o $(BUILD)/ctype.o $(BUILD)/string.o \
	$(BUILD)/time.o $(BUILD)/vsprintf.o

# The stage1 blob must fit what the launcher embeds; xell's own first stage is held to 16 KB.
MAX_SIZE = 16384

all: $(BUILD)/stage1.bin $(BUILD)/stage1.h

$(BUILD):
	@mkdir -p $@

$(BUILD)/main.o: main.c ../include/handoff.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(LV1)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(LV1)/%.S | $(BUILD)
	$(CC) $(AFLAGS) -c -o $@ $<

$(BUILD)/stage1.elf: stage1.lds $(OBJS)
	$(CC) -n -T stage1.lds $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/stage1.bin: $(BUILD)/stage1.elf
	$(OBJCOPY) -O binary $< $@
	@test `stat -L -c %s $@` -le $(MAX_SIZE) || (echo "stage1.bin is over $(MAX_SIZE) bytes"; rm -f $@; exit 1)

$(BUILD)/stage1.h: $(BUILD)/stage1.bin
	cd $(BUILD) && xxd -i -n tinyxell_stage1 stage1.bin > stage1.h

clean:
	rm -rf $(BUILD)

.PHONY: all clean
