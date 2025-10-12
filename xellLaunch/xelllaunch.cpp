//--------------------------------------------------------------------------------------
//	start xell from a xex, required documented hv patch below to work
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include "kernel.h"
#include <stdint.h>

#define NAND_XELL_OFFSET 0x95060
#ifdef __cplusplus
extern "C" {
#endif

	HRESULT __stdcall ObCreateSymbolicLink( STRING*, STRING*);
	HRESULT __stdcall ObDeleteSymbolicLink( STRING* );

	NTSYSAPI
	DWORD
	NTAPI
	XexGetModuleHandle(
		IN		PSZ moduleName,
		IN OUT	PHANDLE hand
		); 

	// ie XexGetProcedureAddress(hand ,0x50, &addr) returns 0 on success
	NTSYSAPI
	DWORD
	NTAPI
	XexGetProcedureAddress(
		IN		HANDLE hand,
		IN		DWORD dwOrdinal,
		IN		PVOID Address
		);

	NTSYSAPI
	PVOID
	NTAPI
	MmGetPhysicalAddress(
		IN		PVOID Address
		);

	NTSYSAPI
	void
	NTAPI
	DbgPrint(
		const char* s,
		...
		);

#ifdef __cplusplus
}
#endif



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
	size_t physicalLength = 0x40000;

    // allocate a buffer for our execute 
    uint8_t *payload_buf = (uint8_t *)XPhysicalAlloc(physicalLength, MAXULONG_PTR, 0, PAGE_READWRITE);
    uint64_t payload_addr = 0x8000000000000000 | (uint64_t)MmGetPhysicalAddress(payload_buf);
    memcpy(payload_buf, code, length);
    
	// Call the FreeBoot backdoor
	HvxGetVersion( HVX_MAGIC_NUMBER, 4, address, payload_addr, physicalLength );

    XPhysicalFree(payload_buf);
}

static LPCWSTR buttons[1] = {L"Fuck!"};
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

char* devices[] = {
	"\\Device\\Mass0\\",
	"\\Device\\Mass1\\",
	"\\Device\\Mass2\\",
	"\\Device\\Harddisk0\\Partition1\\",
	"\\Device\\Cdrom0\\",
};
char fileName[] = "xl:\\xell.bin";

BYTE xelldata[0x40000];
DWORD xellsize;

void startXell(BOOL useNand)
{
//	UINT64 dest = 0x8000000013000000ULL; // xell-ll
//	UINT64 dest = 0x800000001c000000ULL; // xell-1f
	UINT64 dest = 0x800000001c040000ULL; // xell-2f
	UINT64 src = 0x8000000000000000ULL;
	UINT64 len = 0;

	if(useNand)
	{
		//src = 0x80000200C8095060ULL
		src = 0x80000200C8070000ULL;
		len = len+0x10000;
		DbgPrint("syscall - starting xell nand\n");
		HvxExecute(dest, (void *)src, len);
	}
	else
	{
		/*PBYTE xell_buf = (PBYTE)XPhysicalAlloc(0x40000, MAXULONG_PTR, 0, MEM_LARGE_PAGES|PAGE_READWRITE|PAGE_NOCACHE);
		ZeroMemory(xell_buf, 0x40000);
		memcpy(xell_buf, xelldata, xellsize);
		src = src+((DWORD)MmGetPhysicalAddress(xell_buf));
		len = 0+((xellsize/4)& 0xFFFFFFFF);*/
		DbgPrint("syscall - starting xell file\n");
		HvxExecute(dest, (void *)xelldata, xellsize);
	}
}

DWORD readFile(const char* path)
{
	DWORD read = 0;
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE)
		return read;
	ReadFile(file, xelldata, 0x40000, &read, NULL);
	CloseHandle(file);
	return read;
}

VOID __cdecl main()
{
	// Try to load XeLL from one of the files adjacent to the xex
	xellsize = readFile("GAME:\\xell-1f.bin");
	if(xellsize != 0)
		HvxExecute(0x800000001c000000, (void *)xelldata, xellsize);
	
	xellsize = readFile("GAME:\\xell-gggggg.bin");
	if(xellsize != 0)
		HvxExecute(0x800000001c000000, (void *)xelldata, xellsize);

	xellsize = readFile("GAME:\\xell-2f.bin");
	if(xellsize != 0)
		HvxExecute(0x800000001c040000, (void *)xelldata, xellsize);

	// If we couldn't load XeLL from a file adjacent to the xex, look in the
	// root of any attached devices (not the flashfs for now)
	for(int i = 0; i < 5; i++)
	{
		Mount("XL:", devices[i]);

		xellsize = readFile("XL:\\xell-1f.bin");
		if(xellsize != 0)
			HvxExecute(0x800000001c000000, (void *)xelldata, xellsize);
	
		xellsize = readFile("XL:\\xell-gggggg.bin");
		if(xellsize != 0)
			HvxExecute(0x800000001c000000, (void *)xelldata, xellsize);

		xellsize = readFile("XL:\\xell-2f.bin");
		if(xellsize != 0)
			HvxExecute(0x800000001c040000, (void *)xelldata, xellsize);
	}

#if 0
	// TODO fully implement at a later date

	// If we couldn't load XeLL from a file adjacent to the xex, or from a device
	// try from the flash filesystem. Should work for RGLoader and XDKBuild

	// First we need to mount the flash filesystem...
	Mount("FLASH:", "\\Device\\Flash");

	// RGLoader and XDKBuild for non-JTAG stores xell-gggggg.bin in flash
	xellsize = readFile("FLASH:\\xell-gggggg.bin");
	if(xellsize != 0)
		HvxExecute(0x800000001c000000, (void *)xelldata, xellsize);

	// RGLoader for JTAG tbd, we're going to guess it's also in flash but uses xell-2f.bin
	xellsize = readFile("FLASH:\\xell-2f.bin");
	if(xellsize != 0)
		HvxExecute(0x800000001c040000, (void *)xelldata, xellsize);

	// If it's not beside the xex, on a defice, or in the flashfs, try loading from NAND
	// TODO gotta implement this...
	// - is the NAND memory mapped?
	// - Do i have to manually read the pages?
#endif

	MessageBox(L"Couldn't find a suitable XeLL image to load!");

	// all else fails (it'll crash first unless the patches are missing) just drop back to dash
	XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
}
