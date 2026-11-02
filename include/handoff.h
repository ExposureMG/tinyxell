// The handoff between the launcher XEX and tinyxell's stage1. The launcher flattens the homebrew ELF into one image in
// physical memory below the stage1 load address, finds this struct in the stage1 blob by its magic, fills it in, and
// passes the blob to the hypervisor. Stage1 catches the CPUs, then copies the image down to its load address, clears
// its bss and jumps to its entry point. All fields are big-endian, as both sides are.
#ifndef TINYXELL_HANDOFF_H
#define TINYXELL_HANDOFF_H

#include <stdint.h>

#define TINYXELL_HANDOFF_MAGIC 0x54584C48u // "TXLH"
#define TINYXELL_HANDOFF_VERSION 1u

// Where the hypervisor copies stage1 and jumps to: xell-1f's address (xell-reloaded:source/lv1/xell-1f.lds).
#define TINYXELL_STAGE1_ADDR 0x800000001C000000ull
// The image, its bss and the copy's source must all end below this, clear of stage1, its bss at 0x1C080000 and
// its stacks below 0x1E000000 (xell-reloaded:source/lv1/startup.S).
#define TINYXELL_PHYS_LIMIT 0x1C000000u

struct tinyxell_handoff {
	uint32_t magic;
	uint32_t version;
	// Physical address of the flattened image, as the launcher's allocation has it.
	uint32_t source;
	// Bytes of the image to copy, then bytes of zeros after them.
	uint32_t size;
	uint32_t bss_size;
	// Physical address the image runs at; for libxenon titles, 0 (libxenon:devkitxenon/app.lds).
	uint32_t load;
	// Physical address of the entry point; the secondary threads go to entry + 0x60
	// (libxenon:libxenon/startup/xenon/startup_from_xell.S).
	uint32_t entry;
	uint32_t reserved;
};

#endif
