#pragma once
// The crashes written down: to OpenJuice.log with the stack, and as the stock game writes crash.txt.

#include <windows.h>

namespace app {

// A crash written to the log: the exception, then the frames of its stack by the program's symbols.
LONG WINAPI logCrash(EXCEPTION_POINTERS* e);

// The unhandled exceptions' filter of the game's main function (0x005A34D0, not a Ghidra function; set by 0x005A3740):
// crash.txt in the game's folder (0x004F9BE0: the folder less one \ and one / at its end; its file "%s\%s" opened
// to write, 0x004FD540 mode 1: CreateFileA with GENERIC_WRITE, no sharing, CREATE_ALWAYS, normal and sequential, a
// missing folder offered to be made, "Create directory?"), each line written as it is printed: the exception's code,
// flags, address, count of parameters and its first two, the registers and WinMain's address. The search goes on (0).
// x64: the pointers and the registers of the same names (Rdi .. Rsp, Rip for Eip) as their low 32 bits, as %x prints.
// The stock record first, then OpenJuice's own in OpenJuice.log (logCrash, the port's diagnostics).
LONG WINAPI writeCrashTxt(EXCEPTION_POINTERS* e);

// [0x0075DAD8]: the game's folder, the current directory WinMain starts in (0x005A38CA: GetCurrentDirectoryA of 256
// characters); here the folder of OpenJuice.ini's [Game] Path, made current first.
inline char g_gameFolder[0x100] = {};

}  // namespace app
