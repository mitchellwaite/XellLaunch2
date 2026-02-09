#include <xtl.h>
#include <stdio.h>
#include <stdint.h>

#include "kernel_funcs.h"
#include "hv_funcs.h"
#include "xell_2f.h"
#define XELL_2F_DEST 0x800000001c040000

VOID __cdecl main()
{
	if( !HvxIsSyscallZeroBackdoorInstalled() )
	{
		MessageBox(L"FreeBoot syscall 0 backdoor unavailable on this system.");
		goto exit;
	}

	// We're going to use xell-2f as a base for the application
	HvxExecute(XELL_2F_DEST, (void *)xell_2f_bin, xell_2f_bin_len);

exit:
	// all else fails (it'll crash first unless the patches are missing) just drop back to dash
	XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
}
