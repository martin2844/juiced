// The functions of Windows 8 that the toolset's C++ library calls directly: std::filesystem's CreateFile2 and
// CopyFile2 (the debug builds keep the copy, which the release builds leave out unused) and the clocks'
// GetSystemTimePreciseAsFileTime. An executable importing them does not start on Windows 7 (the system finds
// no such entry in kernel32.dll), while OpenJuice targets Windows 7 (_WIN32_WINNT 0x0601, as the build's props
// say). Their import slots are defined here, in the executable's own objects, so the linker takes these instead of
// kernel32.lib's: each calls the system's function where it has one, else does the same with the functions of
// Windows 7 (Windows7.h). The 64-bit slots are below; the 32-bit names (__imp__CreateFile2@20, ..) cannot be spelled
// in C++, so Win7Imports32.asm defines them over the functions here.

#include "Game/Platform/Windows7.h"

#include <windows.h>

namespace {

using CreateFile2Function = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, oj::win7::CreateFile2Parameters*);
using CopyFile2Function = HRESULT(WINAPI*)(PCWSTR, PCWSTR, oj::win7::CopyFile2Parameters*);
using PreciseTimeFunction = void(WINAPI*)(LPFILETIME);

// kernel32's own function of the name; none on Windows 7.
template <class Function>
Function systemFunction(const char* name) {
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    return kernel32 != nullptr ? reinterpret_cast<Function>(GetProcAddress(kernel32, name)) : nullptr;
}

}  // namespace

extern "C" {

HANDLE WINAPI oj_CreateFile2(LPCWSTR name, DWORD access, DWORD share, DWORD disposition,
                             oj::win7::CreateFile2Parameters* parameters) {
    static const auto system = systemFunction<CreateFile2Function>("CreateFile2");
    if (system != nullptr) {
        return system(name, access, share, disposition, parameters);
    }
    return oj::win7::createFile2(name, access, share, disposition, parameters);
}

HRESULT WINAPI oj_CopyFile2(PCWSTR existing, PCWSTR copy, oj::win7::CopyFile2Parameters* parameters) {
    static const auto system = systemFunction<CopyFile2Function>("CopyFile2");
    if (system != nullptr) {
        return system(existing, copy, parameters);
    }
    return oj::win7::copyFile2(existing, copy, parameters);
}

void WINAPI oj_GetSystemTimePreciseAsFileTime(LPFILETIME time) {
    static const auto system = systemFunction<PreciseTimeFunction>("GetSystemTimePreciseAsFileTime");
    if (system != nullptr) {
        system(time);
        return;
    }
    oj::win7::systemTimePrecise(time);
}

#if defined(_M_X64)
// The import slots the library's objects call through, set before any code runs (constant addresses).
CreateFile2Function __imp_CreateFile2 = oj_CreateFile2;
CopyFile2Function __imp_CopyFile2 = oj_CopyFile2;
PreciseTimeFunction __imp_GetSystemTimePreciseAsFileTime = oj_GetSystemTimePreciseAsFileTime;
#endif

}  // extern "C"
