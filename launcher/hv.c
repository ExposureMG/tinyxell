// The hypervisor calls, from XellLaunch2 (xellLaunch/hv_funcs.h, MIT). XellLaunch2 wrote them as MSVC naked functions;
// here they are assembly stubs. xsdk's calling convention passes and returns whole 64-bit registers
// (xsdk:tools/toolchain/build.sh, -mpowerpc64 and -mcall-xsdk), so each argument lands in r3-r7 as the hypervisor reads
// it. Every argument is declared 64-bit so the caller fills the whole register.

#include "hv.h"

#include <string.h>

#include "xsdk/kernel.h"
#include "xsdk/log.h"

#include "hvpp_signed.h"

// HvxGetVersion is the FreeBoot HV backdoor. op does the following:
// 0 and 1 just return 1 (test if the syscall 0 backdoor is installed)
// 2 re-enables memory protection
// 3 disables memory protection
// 4 copies then jumps
// 5 copies
#define HVX_MAGIC_NUMBER 0x72627472

uint64_t hvx_get_version(uint64_t magic, uint64_t op, uint64_t source, uint64_t dest, uint64_t length);
uint64_t hvx_expansion_install(uint64_t address, uint64_t size);
uint64_t hvx_expansion_call(uint64_t sig, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

__asm__(".section .text.hvx_get_version,\"ax\",@progbits\n"
	".globl hvx_get_version\n"
	"hvx_get_version:\n"
	"	li 0, 0x0\n"
	"	sc\n"
	"	blr\n"
	".section .text.hvx_expansion_install,\"ax\",@progbits\n"
	".globl hvx_expansion_install\n"
	"hvx_expansion_install:\n"
	"	li 0, 0x70\n"
	"	sc\n"
	"	blr\n"
	".section .text.hvx_expansion_call,\"ax\",@progbits\n"
	".globl hvx_expansion_call\n"
	"hvx_expansion_call:\n"
	"	li 0, 0x71\n"
	"	sc\n"
	"	blr\n"
	".previous\n");

static bool use_expansion_backdoor;

bool hv_syscall_zero_backdoor_installed(void)
{
	// If op 0 or 1 of HvxGetVersion returns 1, we know the FreeBoot syscall 0 backdoor is installed.
	return hvx_get_version(HVX_MAGIC_NUMBER, 1, 0, 0, 0) == 1;
}

uint64_t hv_install_expansion_backdoor(void)
{
	enum { EXPANSION_BYTES = 0x1000 };
	uint8_t *const alloc = MmAllocatePhysicalMemoryEx(0, EXPANSION_BYTES, PAGE_READWRITE, 0, UINT32_MAX, 0);
	if (!alloc) {
		xsdk_log_write("tinyxell: MmAllocatePhysicalMemoryEx failed for the HV expansion");
		return UINT64_MAX;
	}
	memset(alloc, 0, EXPANSION_BYTES);
	memcpy(alloc, HVPP_signed_bin, HVPP_signed_bin_len);
	const uint64_t ret = hvx_expansion_install(MmGetPhysicalAddress(alloc), EXPANSION_BYTES);
	if (ret == 0)
		use_expansion_backdoor = true;
	MmFreePhysicalMemory(0, alloc);
	return ret;
}

void hv_execute(uint64_t address, const void *code, uint32_t length)
{
	uint8_t *const payload = MmAllocatePhysicalMemoryEx(0, length, PAGE_READWRITE, 0, UINT32_MAX, 0);
	if (!payload) {
		xsdk_log_write("tinyxell: MmAllocatePhysicalMemoryEx failed for the HV payload");
		return;
	}
	const uint64_t payload_address = 0x8000000000000000ull | MmGetPhysicalAddress(payload);
	memcpy(payload, code, length);

	if (use_expansion_backdoor)
		// 0xC = HvExecute, equivalent to option 4 of the freeboot backdoor
		(void)hvx_expansion_call(0x48565050 /* 'HVPP' */, 0xC, address, payload_address, length);
	else
		(void)hvx_get_version(HVX_MAGIC_NUMBER, 4, address, payload_address, length);

	MmFreePhysicalMemory(0, payload);
}
