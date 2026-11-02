// tinyxell stage1: XeLL's first stage cut down to the path XellLaunch takes. It always starts from the hypervisor of a
// running system, so it only catches the CPUs (no SoC init, no wake from power-on), then copies the image the launcher
// left in memory to where it runs and jumps to it. No stage2, no decompression, no search.
//
// The CPU catch and the jump are xell-reloaded's (source/lv1/main.c, start() and execute_elf_at()); startup.S, the
// cache, string, printf and delay helpers are used from the submodule unchanged.

#include "handoff.h"

#include "cache.h"
#include "main.h"
#include "string.h"
#include "time.h"
#include "types.h"
#include "vsprintf.h"

extern char bss_start[], bss_end[];
extern char start_from_rom[];

// startup.S reads these by name.
volatile unsigned long secondary_hold_addr = 1;
volatile int processors_online[6] = { 1 };
volatile long wakeup_cpus = 0;

// Filled in by the launcher before the hypervisor copies stage1 here; in .data, so it's part of the blob.
volatile struct tinyxell_handoff handoff __attribute__((aligned(8))) = {
	.magic = TINYXELL_HANDOFF_MAGIC,
	.version = TINYXELL_HANDOFF_VERSION,
};

void jump(unsigned long dtc, unsigned long kernel_base, unsigned long null, unsigned long reladdr, unsigned long hrmor);
void fix_hrmor(void);

static inline uint64_t ld(volatile void *addr)
{
	uint64_t l;
	asm volatile("ld %0, 0(%1)" : "=r"(l) : "b"(addr));
	return l;
}

static void putch(unsigned char c)
{
	while (!((*(volatile uint32_t *)0x80000200ea001018) & 0x02000000))
		;
	*(volatile uint32_t *)0x80000200ea001014 = (c << 24) & 0xFF000000;
}

int putchar(int c)
{
	if (c == '\n')
		putch('\r');
	putch(c);
	return 0;
}

void putstring(const char *c)
{
	while (*c)
		putchar(*c++);
}

int puts(const char *c)
{
	putstring(c);
	putstring("\n");
	return 0;
}

static int get_online_processors(void)
{
	int res = 0;
	for (int i = 0; i < 6; ++i)
		if (processors_online[i])
			res |= 1 << i;
	return res;
}

static void place_jump(void *addr, void *_target)
{
	unsigned long target = (unsigned long)_target;
	dcache_flush(addr - 0x80, 0x100);
	*(volatile uint32_t *)(addr - 0x18 + 0) = 0x3c600000 | ((target >> 48) & 0xFFFF);
	*(volatile uint32_t *)(addr - 0x18 + 4) = 0x786307c6;
	*(volatile uint32_t *)(addr - 0x18 + 8) = 0x64630000 | ((target >> 16) & 0xFFFF);
	*(volatile uint32_t *)(addr - 0x18 + 0xc) = 0x60630000 | (target & 0xFFFF);
	*(volatile uint32_t *)(addr - 0x18 + 0x10) = 0x7c6803a6;
	*(volatile uint32_t *)(addr - 0x18 + 0x14) = 0x4e800021;
	flush_code(addr - 0x18, 0x18);
	*(volatile uint32_t *)(addr + 0) = 0x4bffffe8;
	flush_code(addr, 0x80);
}

static void halt(const char *why)
{
	printf("[ERROR] %s\n * looping...\n", why);
	for (;;)
		;
}

// Sends the other five threads, which are running the kernel, into start_from_rom, where they wait on
// secondary_hold_addr (xell-reloaded:source/lv1/main.c, start()).
static void catch_cpus(unsigned long hrmor)
{
	static const int exc[] = { EX_RESET, EX_MACHINE_CHECK, EX_DSI, EX_DATA_SEGMENT, EX_ISI, EX_INSTRUCTION_SEGMENT,
		EX_INTERRUPT, EX_ALIGNMENT, EX_PROGRAM, EX_FLOATING_POINT, EX_DECREMENTER, 0x980, EX_SYSTEM_CALL, EX_TRACE,
		EX_PERFORMANCE, 0xF20, EX_IABR, 0x1600, EX_THERMAL, 0x1800 };

	printf(" * Attempting to catch all CPUs...\n");

	for (unsigned i = 0; i < sizeof(exc) / sizeof(*exc); ++i)
		place_jump((void *)hrmor + exc[i], start_from_rom);
	place_jump((void *)0x8000000000000700, start_from_rom);

	while (get_online_processors() != 0x3f) {
		printf("CPUs online: %02x..\n", get_online_processors());
		mdelay(10);

		for (int i = 1; i < 6; ++i) {
			*(volatile uint64_t *)(0x8000020000050070ULL + i * 0x1000) = 0x7c;
			*(volatile uint64_t *)(0x8000020000050068ULL + i * 0x1000) = 0;
			(void)*(volatile uint64_t *)(0x8000020000050008ULL + i * 0x1000);
			while (*(volatile uint64_t *)(0x8000020000050050ULL + i * 0x1000) != 0x7C)
				;
		}

		// IPI request
		*(volatile uint64_t *)(0x8000020000052010ULL) = 0x3e0078;
	}

	fix_hrmor();

	// Re-reset the interrupt controllers, removing their pending IPIs.
	for (int i = 1; i < 6; ++i) {
		*(volatile uint64_t *)(0x8000020000050068ULL + i * 0x1000) = 0x74;
		while (*(volatile uint64_t *)(0x8000020000050050ULL + i * 0x1000) != 0x7C)
			;
	}

	printf("CPUs online: %02x..\n * success.\n", get_online_processors());
}

// A forward copy, safe when dest <= src even if they overlap. xell-reloaded's string.c has its memmove under #if 0, and
// the Makefile's -fno-tree-loop-distribute-patterns keeps GCC from turning this loop into a memcpy call.
static void copy_down(void *dest, const void *src, unsigned long count)
{
	if ((((unsigned long)dest | (unsigned long)src) & 7) == 0) {
		uint64_t *d = dest;
		const uint64_t *s = src;
		for (; count >= 8; count -= 8)
			*d++ = *s++;
		dest = d;
		src = s;
	}
	unsigned char *d = dest;
	const unsigned char *s = src;
	while (count--)
		*d++ = *s++;
}

// Copies the image down and runs it. The launcher has checked that the image, its bss and the source all end below
// TINYXELL_PHYS_LIMIT and that load <= source, so the forward copy never overwrites what it has yet to read.
static void run_image(void)
{
	if (handoff.magic != TINYXELL_HANDOFF_MAGIC || handoff.version != TINYXELL_HANDOFF_VERSION)
		halt("bad handoff");
	if (handoff.size == 0 || handoff.load > handoff.source)
		halt("bad image placement");

	void *source = (void *)(0x8000000000000000UL | handoff.source);
	void *target = (void *)(0x8000000000000000UL | handoff.load);
	unsigned long entry = handoff.entry;

	printf(" * Copying %08x bytes from %08x to %08x, clearing %08x\n", (unsigned)handoff.size,
		(unsigned)handoff.source, (unsigned)handoff.load, (unsigned)handoff.bss_size);

	copy_down(target, source, handoff.size);
	memset(target + handoff.size, 0, handoff.bss_size);
	flush_code(target, handoff.size + handoff.bss_size);

	printf(" * GO (entrypoint: %08lx)\n", entry);

	// As execute_elf_at() does: the secondary threads go to entry + 0x60, the primary to entry, in real mode.
	secondary_hold_addr = entry | 0x8000000000000060UL;
	jump(0, entry, 0, entry, 0);
}

int start(int pir, unsigned long hrmor, unsigned long pvr)
{
	(void)pir;
	(void)pvr;

	secondary_hold_addr = 0;

	// Initialize BSS first. DO NOT INSERT CODE BEFORE THIS!
	memset(bss_start, 0, bss_end - bss_start);

	// UART at 115200, 8, N, 1
	*(volatile uint32_t *)0x80000200ea00101c = 0xe6010000;

	printf("\ntinyxell - stage1\n");

	// Thread 0's interrupt controller is set up only on a running system; tinyxell has nothing to do otherwise.
	if (ld((void *)0x8000020000050000 + 0x2070) != 0x7C)
		halt("not started from a running system");

	catch_cpus(hrmor);
	run_image();
	halt("jump returned");
	return 0;
}

int main(void)
{
	return 0;
}
