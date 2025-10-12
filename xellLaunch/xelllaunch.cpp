//--------------------------------------------------------------------------------------
//	start xell from a xex, required documented hv patch below to work
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include "kernel.h"
#include <stdint.h>

extern "C" {
	HRESULT	__stdcall ObCreateSymbolicLink( STRING*, STRING*);
	HRESULT __stdcall ObDeleteSymbolicLink( STRING* );
	DWORD	__stdcall XexGetModuleHandle( char * moduleName, HANDLE * handle );
	// ie XexGetProcedureAddress(hand ,0x50, &addr) returns 0 on success
	DWORD	__stdcall XexGetProcedureAddress( HANDLE handle, DWORD dwOrdinal, void * address );
	void *	__stdcall MmGetPhysicalAddress( void * address );
	void	__stdcall DbgPrint(	const char* s, ... );
}

HRESULT Mount(PCHAR szDrive, PCHAR szDevice)
{
	CHAR szDestinationDrive[MAX_PATH];
	sprintf_s(szDestinationDrive, MAX_PATH, "\\??\\%s", szDrive);
	STRING DeviceName = MAKE_STRING(szDevice);
	STRING LinkName = MAKE_STRING(szDestinationDrive);
	ObDeleteSymbolicLink(&LinkName);
	return (HRESULT)ObCreateSymbolicLink(&LinkName, &DeviceName);
}

// HvxGetVersion is the FreeBoot HV backdoor. op does the following:
// 0 and 1 just return 1
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

static LPCWSTR buttons[1] = {L"OK"};
static MESSAGEBOX_RESULT result;
static XOVERLAPPED overlapped;
static void MessageBox(wchar_t *text)
{
    if (XShowMessageBoxUI(XUSER_INDEX_ANY, L"XellLaunch2 Error", text, 1, buttons, 0, XMB_ERRORICON, &result, &overlapped) == ERROR_IO_PENDING)
    {
        while (!XHasOverlappedIoCompleted(&overlapped))
            Sleep(50);
    }
}

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
char* xellBinaryNameArr[] = {
	"xell-1f.bin",
	"xell-2f.bin",
	"xell-gggggg.bin",
	"xell-gggggg_cygnos_demon.bin",
	"xell-1f_cygnos_demon.bin",
	"xell-2f_cygnos_demon"
};
#define xellBinaryNameArrLen 6

int xellNandOffsets[] = { 0x70000,    // Glitch, Glitch2, Glitch2m, DevGL: xell-gggggg
                          0x95060,    // JTAG: xell-2f
                          0x100000,   // XeLL-Only Image (Main XeLL)
                          0xC0000,    // XeLL-Only Image (Backup XeLL)
                          0xE0000,    // Unknown, but listed in libxenon updxell function
                          0xB80000 }; // Unknown, but listed in libxenon updxell function

#define XELL_DEST 0x800000001c000000
#define XELL_2F_DEST 0x800000001c040000
#define XELL_BINARY_LEN 0x40000

BYTE xelldata[XELL_BINARY_LEN];
DWORD xellsize;

DWORD readFile(const char* path)
{
	DWORD read = 0;
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE)
		return read;
	ReadFile(file, xelldata, XELL_BINARY_LEN, &read, NULL);
	CloseHandle(file);
	return read;
}

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

void tryLoadXell(char * drive)
{
	char xellLoadPath[MAX_PATH];

	for( int i = 0 ; i < xellBinaryNameArrLen; i++ )
	{
		// Construct a path from the specified drive and the list of xell binaries
		sprintf_s(xellLoadPath,MAX_PATH,"%s\\%s",drive,xellBinaryNameArr[i]);
		xellsize = readFile(xellLoadPath);

		// Xell binaries should always be 256kb. Anything else is a corrupt file
		// if this changes, XELL_BINARY_LEN will need to be updated, or perhaps
		// a function implementation that can validate multiple sizes can be added
		if(xellsize == XELL_BINARY_LEN && validateXellHeader(xelldata))
		{
			// xell-2f uses a different destination than the others.
			// if the right dest isn't used, XeLL will hang
			if(NULL != strstr(xellBinaryNameArr[i],"xell-2f"))
			{
				HvxExecute(XELL_2F_DEST, (void *)xelldata, xellsize);
			}
			else
			{
				HvxExecute(XELL_DEST, (void *)xelldata, xellsize);
			}
		}
	}
}

VOID __cdecl main()
{
	// Try to load XeLL from one of the files adjacent to the xex
	tryLoadXell("GAME:");

	// If we couldn't load XeLL from a file adjacent to the xex, look in the
	// root of any attached devices (not the flashfs for now)
	for(int i = 0; i < xellDeviceSearchPathArrLen; i++)
	{
		Mount("XL:", xellDeviceSearchPathArr[i]);
		tryLoadXell("XL:");
	}

	// If we couldn't load XeLL from a file adjacent to the xex, or from a device
	// try from the flash filesystem. Should work for RGLoader and XDKBuild
	// TODO: this hangs at a black screen on my RGLoader machine, but it boots
	// XeLL fine from GAME:, USB, or the eject button. Figure it out later.
#if 0
	Mount("FLASH:", "\\Device\\Flash");
	tryLoadXell("FLASH:");
#endif

	// If it's not beside the xex, on a defice, or in the flashfs, try loading from NAND
	// TODO gotta implement this...
	// - is the NAND memory mapped?
	// - Do i have to manually read the pages?

	MessageBox(L"Couldn't find a suitable XeLL image to load!");

	// all else fails (it'll crash first unless the patches are missing) just drop back to dash
	XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
}
