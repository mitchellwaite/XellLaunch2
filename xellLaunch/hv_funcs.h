#include "kernel_funcs.h"
#include <stdint.h>

#ifndef _XELLLAUNCH_HV_FUNCS_H
#define _XELLLAUNCH_HV_FUNCS_H

// HvxGetVersion is the FreeBoot HV backdoor. op does the following:
// 0 and 1 just return 1 (test if the syscall 0 backdoor is installed)
// 2 re-enables memory protection
// 3 disables memory protection
// 4 copies then jumps
// 5 copies
//
// All we really need for XellLaunch is option 4, to copy data to HV space and jump to XeLL
//
#define HVX_MAGIC_NUMBER 0x72627472

uint64_t __declspec(naked) HvxGetVersion(uint32_t magic, int op, uint64_t source, uint64_t dest, uint64_t length) {
    __asm
    {
        li r0, 0x0
        sc
        blr
    }
}

void HvxExecute(uint64_t address, void *code, size_t length)
{
    // allocate a buffer for our execute 
    uint8_t *payload_buf = (uint8_t *)XPhysicalAlloc(length, MAXULONG_PTR, 0, PAGE_READWRITE);
    uint64_t payload_addr = 0x8000000000000000 | (uint64_t)MmGetPhysicalAddress(payload_buf);
    memcpy(payload_buf, code, length);
    
	// Call the FreeBoot backdoor
	HvxGetVersion( HVX_MAGIC_NUMBER, 4, address, payload_addr, length );

    XPhysicalFree(payload_buf);
}

bool HvxIsSyscallZeroBackdoorInstalled()
{
	// If op 0 or 1 of HvxGetVersion returns 1, we know the
	// FreeBoot syscall 0 backdoor is installed and can proceed
	if( HvxGetVersion( HVX_MAGIC_NUMBER, 1, NULL, NULL, 0 ) == 1 )
	{
		return true;
	}
	else
	{
		return false;
	}
}

#endif