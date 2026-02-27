#include <xtl.h>
#include <stdio.h>
#include <stdint.h>

#include "kernel_funcs.h"
#include "hv_funcs.h"
#include "xell_2f.h"
//#define DEBUG_MSGBOX 1

// Where we're going to search, if XeLL isn't found in GAME:
char* xellDeviceSearchPathArr[] = {
	"\\Device\\Mass0\\",
	"\\Device\\Mass1\\",
	"\\Device\\Mass2\\",
	"\\Device\\Harddisk0\\Partition1\\",
	"\\Device\\Cdrom0\\",
};
#define xellDeviceSearchPathArrLen 5

// All possible XeLL binaries that the XeLL build can produce
// xell-gggggg is commented out since the soc init code hangs
// when launched from an already running system. Fix TBD
char* xellBinaryNameArr[] = {
	"xell-1f.bin",
	"xell-2f.bin",
	"xell-gggggg.bin",
	"xell-gggggg_cygnos_demon.bin",
	"xell-1f_cygnos_demon.bin",
	"xell-2f_cygnos_demon.bin"
};
#define xellBinaryNameArrLen 6

// Known locations of the XeLL binary in various NAND image types,
// borrowed from the libxenon updxell() function. xell-gggggg is
// commented out since the soc init code hangs when launched from
// an already running system.
int xellNandOffsetsArr[] = { 0x70000,  // Glitch, Glitch2, Glitch2m, DevGL: xell-gggggg
                             0x95060,    // JTAG: xell-2f
		   				     // We PROBABLY won't ever be looking here if we're running XellLaunch
			   			     // but we might as well have them in the list just in case
                             0x100000,   // XeLL-Only Image (Main XeLL)
                             0xC0000,    // XeLL-Only Image (Backup XeLL)
                             0xE0000,    // Unknown, but listed in libxenon updxell function
                             0xB80000 }; // Unknown, but listed in libxenon updxell function
#define xellNandOffsetsArrLen 6

#define XELL_DEST 0x800000001c000000
#define XELL_2F_DEST 0x800000001c040000
#define XELL_BINARY_LEN 0x40000

BYTE xelldata[XELL_BINARY_LEN];
DWORD xellsize;

// Sanity check on the XeLL buffer we're trying to load. If the header bytes are
// present in the buffer, we can be reasonably certain that we've loaded some
// flavour of XeLL. J-Runner does a similar check when injecting XeLL.
bool validateXellHeader(BYTE * xellBuf)
{
	BYTE xellHeaderBytes[] = {0x48, 0x00, 0x00, 0x20, 0x48, 0x00, 0x00, 0xEC, 0x48, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00 };

	if( 0 == memcmp(xellBuf, xellHeaderBytes, 0x10))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void tryLoadXellFromFilesystem(char * drive)
{
	char xellLoadPath[MAX_PATH];

	for( int i = 0 ; i < xellBinaryNameArrLen; i++ )
	{
		// Construct a path from the specified drive and the list of xell binaries
		sprintf_s(xellLoadPath,MAX_PATH,"%s\\%s",drive,xellBinaryNameArr[i]);
		xellsize = readFile(xellLoadPath, xelldata, XELL_BINARY_LEN);

		// Xell binaries should always be 256kb. Anything else is a corrupt file
		// if this changes, XELL_BINARY_LEN will need to be updated, or perhaps
		// a function implementation that can validate multiple sizes can be added
		if(xellsize == XELL_BINARY_LEN && validateXellHeader(xelldata))
		{
			// xell-2f uses a different destination than the others.
			// if the right dest isn't used, XeLL will hang
			if(NULL != strstr(xellBinaryNameArr[i],"xell-2f"))
			{
#ifdef DEBUG_MSGBOX
				MessageBox(L"Found xell-2f on disk!");
#endif
				HvxExecute(XELL_2F_DEST, (void *)xelldata, xellsize);
			}
			else
			{
#ifdef DEBUG_MSGBOX
				MessageBox(L"Found XeLL other than 2f on disk!");
#endif
				HvxExecute(XELL_DEST, (void *)xelldata, xellsize);
			}
		}
	}
}

void tryLoadXellFromNandOffset()
{
	HANDLE hFile;
	OBJECT_ATTRIBUTES atFlash;
	IO_STATUS_BLOCK ioFlash;
	DWORD dwPos;
	DWORD returnstatus;
	STRING nFlash = MAKE_STRING("\\Device\\Flash");
	atFlash.RootDirectory = 0;
	atFlash.ObjectName = &nFlash;
	atFlash.Attributes = FILE_ATTRIBUTE_DEVICE;

	// Open the flash as a raw device
	returnstatus = NtOpenFile(&hFile, GENERIC_READ, &atFlash, &ioFlash, OPEN_EXISTING, FILE_SYNCHRONOUS_IO_NONALERT);

	if (returnstatus != 0)
	{
		return;
	}

	for(int i = 0; i<sizeof(xellNandOffsetsArr); i++)
	{
		dwPos = SetFilePointer(hFile, xellNandOffsetsArr[i], NULL, FILE_BEGIN);

		if( dwPos != INVALID_SET_FILE_POINTER )
		{
			xellsize = 0;
			ReadFile(hFile, xelldata, XELL_BINARY_LEN, &xellsize, NULL);

			// Xell binaries should always be 256kb. Anything else is a corrupt file
			// if this changes, XELL_BINARY_LEN will need to be updated, or perhaps
			// a function implementation that can validate multiple sizes can be added
			if(xellsize == XELL_BINARY_LEN && validateXellHeader(xelldata))
			{
				// xell-2f uses a different destination than the others.
				// if the right dest isn't used, XeLL will hang
				if(0x95060 == xellNandOffsetsArr[i])
				{
#ifdef DEBUG_MSGBOX
				MessageBox(L"Found xell-2f XeLL in NAND!");
#endif
					HvxExecute(XELL_2F_DEST, (void *)xelldata, xellsize);
				}
				else
				{
#ifdef DEBUG_MSGBOX
				MessageBox(L"Found XeLL other than 2f in NAND!");
#endif
					HvxExecute(XELL_DEST, (void *)xelldata, xellsize);
				}
			}
		}
	}

	// If we've reached here, we couldn't find XeLL in flash (are we on a real devkit???)
	// Close the flash file handle
	NtClose(hFile);
}

VOID __cdecl main()
{
	if( !HvxIsSyscallZeroBackdoorInstalled() )
	{
		MessageBox(L"FreeBoot syscall 0 backdoor unavailable on this system.");
		goto exit;
	}

	// Try to load XeLL from one of the files adjacent to the xex
	tryLoadXellFromFilesystem("GAME:");

	// If we couldn't load XeLL from a file adjacent to the xex, look in the
	// root of any attached devices (not the flashfs for now)
	for(int i = 0; i < xellDeviceSearchPathArrLen; i++)
	{
		MountDrive("XL:", xellDeviceSearchPathArr[i]);
		tryLoadXellFromFilesystem("XL:");
	}

	// If we couldn't load XeLL from a file adjacent to the xex, or from a device
	// try from the flash filesystem.
	MountDrive("Flash:", "\\Device\\Flash");
	tryLoadXellFromFilesystem("Flash:");

	// If we couldn't load XeLL from the flash filesystem, try to load it from
	// a list of known NAND offsets.
	tryLoadXellFromNandOffset();

	MessageBox(L"Couldn't find a suitable XeLL image to load... we're gonna try the embedded xell-2f.");

	HvxExecute(XELL_2F_DEST, (void *)xell_2f_bin, xell_2f_bin_len);

exit:
	// all else fails (it'll crash first unless the patches are missing) just drop back to dash
	XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
}
