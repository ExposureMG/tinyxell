#ifndef TINYXELL_HV_H
#define TINYXELL_HV_H

#include <stdbool.h>
#include <stdint.h>

// Whether FreeBoot's syscall 0 backdoor answers, as on RGH/JTAG images and RGLoader.
bool hv_syscall_zero_backdoor_installed(void);
// Installs RGLoader's signed HVPP expansion, for systems without the syscall 0 backdoor; 0 on success.
uint64_t hv_install_expansion_backdoor(void);
// Has the hypervisor copy length bytes of code to address and jump there; returns only if that failed.
void hv_execute(uint64_t address, const void *code, uint32_t length);

#endif
