// The functions of Windows 8 that the toolset's C++ library calls directly: std::filesystem's CreateFile2 and
// CopyFile2 (the debug builds keep the copy, which the release builds leave out unused), the clocks'
// GetSystemTimePreciseAsFileTime, and the atomic waits' WaitOnAddress and WakeByAddressAll (the fixed build's OpenAL
// Soft). An executable importing them does not start on Windows 7 (the system finds
// no such entry in kernel32.dll), while OpenJuice targets Windows 7 (_WIN32_WINNT 0x0601, as the build's props
// say). Their import slots are defined here, in the executable's own objects and read-only as the system's are, so
// the linker takes these instead of kernel32.lib's: each holds the function that does the same with the functions of
// Windows 7 (Windows7.h), on every Windows alike, nothing looked up as the game runs. The 64-bit slots are below; the
// 32-bit names (__imp__CreateFile2@20, ..) cannot be spelled in C++, so Win7Imports32.asm defines them over the
// functions here.

#include "Game/Platform/Windows7.h"

#include <windows.h>

namespace {

using CreateFile2Function = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, oj::win7::CreateFile2Parameters*);
using CopyFile2Function = HRESULT(WINAPI*)(PCWSTR, PCWSTR, oj::win7::CopyFile2Parameters*);
using PreciseTimeFunction = void(WINAPI*)(LPFILETIME);
using WaitOnAddressFunction = BOOL(WINAPI*)(volatile VOID*, PVOID, SIZE_T, DWORD);
using WakeByAddressFunction = void(WINAPI*)(PVOID);

}  // namespace

extern "C" {

HANDLE WINAPI oj_CreateFile2(LPCWSTR name, DWORD access, DWORD share, DWORD disposition,
                             oj::win7::CreateFile2Parameters* parameters) {
    return oj::win7::createFile2(name, access, share, disposition, parameters);
}

HRESULT WINAPI oj_CopyFile2(PCWSTR existing, PCWSTR copy, oj::win7::CopyFile2Parameters* parameters) {
    return oj::win7::copyFile2(existing, copy, parameters);
}

void WINAPI oj_GetSystemTimePreciseAsFileTime(LPFILETIME time) {
    oj::win7::systemTimePrecise(time);
}

BOOL WINAPI oj_WaitOnAddress(volatile VOID* address, PVOID compare, SIZE_T size, DWORD milliseconds) {
    return oj::win7::waitOnAddress(address, compare, size, milliseconds) ? TRUE : FALSE;
}

void WINAPI oj_WakeByAddressAll(PVOID address) {
    oj::win7::wakeByAddressAll(address);
}

// The functions themselves too (synchronization.lib's import of them, called directly, would bring its slots in
// beside the ones below): declared by the headers for Windows 8 and later only.
BOOL WINAPI WaitOnAddress(volatile VOID* address, PVOID compare, SIZE_T size, DWORD milliseconds) {
    return oj_WaitOnAddress(address, compare, size, milliseconds);
}

void WINAPI WakeByAddressAll(PVOID address) {
    oj_WakeByAddressAll(address);
}

#if defined(_M_X64)
// The import slots the library's objects call through: constant, in the read-only data.
extern const CreateFile2Function __imp_CreateFile2 = oj_CreateFile2;
extern const CopyFile2Function __imp_CopyFile2 = oj_CopyFile2;
extern const PreciseTimeFunction __imp_GetSystemTimePreciseAsFileTime = oj_GetSystemTimePreciseAsFileTime;
extern const WaitOnAddressFunction __imp_WaitOnAddress = oj_WaitOnAddress;
extern const WakeByAddressFunction __imp_WakeByAddressAll = oj_WakeByAddressAll;
#endif

}  // extern "C"
