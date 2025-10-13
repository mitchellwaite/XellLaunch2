# XellLaunch2

An updated version of XellLaunch, based on the publicly available source code. Uses the HvxGetVersion/Syscall 0/FreeBoot backdoor on modified 360's to load and jump to XeLL

It supports loading of any build produced by the upstream xell-reloaded repository. It will search these filenames in the order shown below.

- xell-1f.bin
- xell-2f.bin
- xell-1f_cygnos_demon.bin
- xell-2f_cygnos_demon

When looking for a binary to load, XellLaunch2 will check the following locations in the order shown:

- `GAME:` (Adjacent to XellLaunch2.xex)
- Any attached USB drives (`\\Device\\Mass0`, `\\Device\\Mass1`, etc.)
- The hard drive (`\\Device\\Harddisk0\\Partition1\\`)
- A disc in the DVD drive (`\\Device\\Cdrom0\\`)
- The flash filesystem (`\\Device\\Flash`), XDKBuild and RGLoader store XeLL there
- The logical NAND offset as used for JTAG images
- If no suitable XeLL binary is found, it will launch a version of XeLL-2f embedded in XellLaunch2.xex


A sanity check will be done on the header of each binary to ensure we're at least trying to load something that looks like XeLL.

If no suitable XeLL binary was found, then an error message will be displayed and we'll be kicked back to the dash.

Note, it does NOT support `xell-gggggg.bin` or loading from NAND on a Glitch/Glitch2/DevGL image as the current version of XeLL hangs when started from a running system. I suspect the soc_init code is the culprit. If you figure that out, please submit a patch to the Free60 XeLL reloaded repo.

## Req's to build

- Visual Studio 2010
- Xbox 360 SDK

Clone the repository, open the `.sln`, and hit build. XellLaunch2.xex (for devkits and the like) and XellLaunch2_retail.xex (for retail systems, no restrictions) will be produced.

## Future items to work on

- XDKbuild support (it's missing the syscall 0 backdoor)
- `xell-gggggg.bin` support, need to figure out the soc_init issue

## Credits

- cOz for the original DashLaunch/XellLaunch implementation
- InvoxiPlayGames for FreeMyXe, which I referenced to put this together. https://github.com/FreeMyXe/FreeMyXe/blob/a6a2d62719b59f87402912249131691a060f4aa9/source/FreeMyXe.c#L389