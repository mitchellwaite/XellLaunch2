# XellLaunch2

An updated version of XellLaunch, based on the publicly available source code. Uses the HvxGetVersion/Syscall 0/FreeBoot backdoor on modified 360's to load and jump to XeLL

It supports loading of any build produced by the upstream xell-reloaded repository. It will search these filenames in the order shown below.

- xell-1f.bin
- xell-2f.bin
- xell-gggggg.bin
- xell-gggggg_cygnos_demon.bin
- xell-1f_cygnos_demon.bin
- xell-2f_cygnos_demon

When looking for a binary to load, XellLaunch2 will check the following locations in the order shown:

- `GAME:` (Adjacent to XellLaunch2.xex)
- Any attached USB drives (`\\Device\\Mass0`, `\\Device\\Mass1`, etc.)
- The hard drive (`\\Device\\Harddisk0\\Partition1\\`)
- A disc in the DVD drive (`\\Device\\Cdrom0\\`)
- TODO: The flash filesystem (`\\Device\\Flash`), XDKBuild and RGLoader store XeLL there
- TODO: The logical NAND offset as used for JTAG, Glitch, Glitch2, Glitch2m, DEVGL images

A sanity check will be done on the header of each binary to ensure we're at least trying to load something that looks like XeLL.

If no suitable XeLL binary was found, then an error message will be displayed and we'll be kicked back to the dash.

Note, it does NOT support `xell.bin` as a renamed XeLL binary like the old XellLaunch, because loading XeLL in the wrong destination will make it hang at a black screen. I've run in to this too many times to count.

## Req's to build

- Visual Studio 2010
- Xbox 360 SDK

Clone the repository, open the `.sln`, and hit build. XellLaunch2.xex should be produced.

## Credits

- cOz for the original DashLaunch/XellLaunch implementation
- InvoxiPlayGames for FreeMyXe, which I referenced to put this together. https://github.com/FreeMyXe/FreeMyXe/blob/a6a2d62719b59f87402912249131691a060f4aa9/source/FreeMyXe.c#L389