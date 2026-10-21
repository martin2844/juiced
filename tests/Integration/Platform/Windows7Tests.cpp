// Integration tests: Windows7.
// OpenJuice runs on Windows 7 (the build targets _WIN32_WINNT 0x0601). The executables of the tests' own configuration
// (beside them: OpenJuice64.exe, OpenJuice.exe, their _d, and OpenJuiceConfig's) are checked as the system's loader
// sees them: each one's header asks for Windows 6.1 at most and every function it imports is one Windows 7 has. The
// list of those is checked by the compiler: each name is taken from the SDK's headers as they declare it for 0x0601,
// and a function of a later Windows is not declared there, so a wrong entry does not build. An import not on the list
// fails with its name: a function of a later Windows needs a way round (Game/Platform/Win7Imports.cpp), one of
// Windows 7 its entry here. The functions the game runs in place of Windows 8's (Game/Platform/Windows7.h) are
// checked doing what those do.

#include "Framework/Test.h"

#include "Game/Platform/Windows7.h"

// clang-format off
#include <winsock2.h>
#include <windows.h>
// clang-format on
#include <d3d9.h>
#include <dbghelp.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <mmreg.h>
#include <dsound.h>
#include <objbase.h>
#include <oleauto.h>
#include <shlobj.h>
#include <commctrl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// A function Windows 7 has: imported by name, or by its ordinal (WS2_32's, DSOUND's and OLEAUT32's, by their import
// libraries).
struct Windows7Function {
    const char* dll;
    const char* name;
    uint16_t ordinal;  // 0: imported by name
};

// Each entry's function taken as the headers declare it for Windows 7 (not called, not linked).
#define OJ_WIN7(dll, function) Windows7Function{dll, (static_cast<void>(sizeof(&::function)), #function), 0}
#define OJ_WIN7_ORDINAL(dll, ordinal, function) \
    Windows7Function{dll, (static_cast<void>(sizeof(&::function)), #function), ordinal}

// What the executables import (Release and Debug, x86 and x64).
const Windows7Function kWindows7[] = {
    // KERNEL32.dll
    OJ_WIN7("KERNEL32.dll", AcquireSRWLockExclusive),
    OJ_WIN7("KERNEL32.dll", AcquireSRWLockShared),
    OJ_WIN7("KERNEL32.dll", AreFileApisANSI),
    OJ_WIN7("KERNEL32.dll", CloseHandle),
    OJ_WIN7("KERNEL32.dll", CompareStringEx),
    OJ_WIN7("KERNEL32.dll", CompareStringW),
    OJ_WIN7("KERNEL32.dll", CopyFileExW),
    OJ_WIN7("KERNEL32.dll", CreateDirectoryA),
    OJ_WIN7("KERNEL32.dll", CreateDirectoryExW),
    OJ_WIN7("KERNEL32.dll", CreateDirectoryW),
    OJ_WIN7("KERNEL32.dll", CreateFileA),
    OJ_WIN7("KERNEL32.dll", CreateFileW),
    OJ_WIN7("KERNEL32.dll", CreateHardLinkW),
    OJ_WIN7("KERNEL32.dll", CreateMutexA),
    OJ_WIN7("KERNEL32.dll", CreateProcessA),
    OJ_WIN7("KERNEL32.dll", CreateSymbolicLinkW),
    OJ_WIN7("KERNEL32.dll", CreateThread),
    OJ_WIN7("KERNEL32.dll", DecodePointer),
    OJ_WIN7("KERNEL32.dll", DeleteCriticalSection),
    OJ_WIN7("KERNEL32.dll", DeleteFileW),
    OJ_WIN7("KERNEL32.dll", DeviceIoControl),
    OJ_WIN7("KERNEL32.dll", EncodePointer),
    OJ_WIN7("KERNEL32.dll", EnterCriticalSection),
    OJ_WIN7("KERNEL32.dll", EnumSystemLocalesW),
    OJ_WIN7("KERNEL32.dll", ExitProcess),
    OJ_WIN7("KERNEL32.dll", ExitThread),
    OJ_WIN7("KERNEL32.dll", FileTimeToSystemTime),
    OJ_WIN7("KERNEL32.dll", FindClose),
    OJ_WIN7("KERNEL32.dll", FindFirstFileExW),
    OJ_WIN7("KERNEL32.dll", FindFirstFileW),
    OJ_WIN7("KERNEL32.dll", FindNextFileW),
    OJ_WIN7("KERNEL32.dll", FlsAlloc),
    OJ_WIN7("KERNEL32.dll", FlsFree),
    OJ_WIN7("KERNEL32.dll", FlsGetValue),
    OJ_WIN7("KERNEL32.dll", FlsSetValue),
    OJ_WIN7("KERNEL32.dll", FlushFileBuffers),
    OJ_WIN7("KERNEL32.dll", FormatMessageA),
    OJ_WIN7("KERNEL32.dll", FreeEnvironmentStringsW),
    OJ_WIN7("KERNEL32.dll", FreeLibrary),
    OJ_WIN7("KERNEL32.dll", FreeLibraryAndExitThread),
    OJ_WIN7("KERNEL32.dll", GetACP),
    OJ_WIN7("KERNEL32.dll", GetCPInfo),
    OJ_WIN7("KERNEL32.dll", GetCommandLineA),
    OJ_WIN7("KERNEL32.dll", GetCommandLineW),
    OJ_WIN7("KERNEL32.dll", GetConsoleMode),
    OJ_WIN7("KERNEL32.dll", GetConsoleOutputCP),
    OJ_WIN7("KERNEL32.dll", GetCurrentDirectoryA),
    OJ_WIN7("KERNEL32.dll", GetCurrentDirectoryW),
    OJ_WIN7("KERNEL32.dll", GetCurrentProcess),
    OJ_WIN7("KERNEL32.dll", GetCurrentProcessId),
    OJ_WIN7("KERNEL32.dll", GetCurrentThread),
    OJ_WIN7("KERNEL32.dll", GetCurrentThreadId),
    OJ_WIN7("KERNEL32.dll", GetDateFormatW),
    OJ_WIN7("KERNEL32.dll", GetDiskFreeSpaceExW),
    OJ_WIN7("KERNEL32.dll", GetDriveTypeW),
    OJ_WIN7("KERNEL32.dll", GetEnvironmentStringsW),
    OJ_WIN7("KERNEL32.dll", GetExitCodeProcess),
    OJ_WIN7("KERNEL32.dll", GetExitCodeThread),
    OJ_WIN7("KERNEL32.dll", GetFileAttributesExW),
    OJ_WIN7("KERNEL32.dll", GetFileAttributesW),
    OJ_WIN7("KERNEL32.dll", GetFileInformationByHandle),
    OJ_WIN7("KERNEL32.dll", GetFileInformationByHandleEx),
    OJ_WIN7("KERNEL32.dll", GetFileSize),
    OJ_WIN7("KERNEL32.dll", GetFileSizeEx),
    OJ_WIN7("KERNEL32.dll", GetFileType),
    OJ_WIN7("KERNEL32.dll", GetFinalPathNameByHandleW),
    OJ_WIN7("KERNEL32.dll", GetFullPathNameW),
    OJ_WIN7("KERNEL32.dll", GetLastError),
    OJ_WIN7("KERNEL32.dll", GetLocalTime),
    OJ_WIN7("KERNEL32.dll", GetLocaleInfoEx),
    OJ_WIN7("KERNEL32.dll", GetLocaleInfoW),
    OJ_WIN7("KERNEL32.dll", GetLogicalProcessorInformationEx),
    OJ_WIN7("KERNEL32.dll", GetModuleFileNameW),
    OJ_WIN7("KERNEL32.dll", GetModuleHandleA),
    OJ_WIN7("KERNEL32.dll", GetModuleHandleExW),
    OJ_WIN7("KERNEL32.dll", GetModuleHandleW),
    OJ_WIN7("KERNEL32.dll", GetOEMCP),
    OJ_WIN7("KERNEL32.dll", GetPrivateProfileStringW),
    OJ_WIN7("KERNEL32.dll", GetProcAddress),
    OJ_WIN7("KERNEL32.dll", GetProcessAffinityMask),
    OJ_WIN7("KERNEL32.dll", GetProcessHeap),
    OJ_WIN7("KERNEL32.dll", GetStartupInfoW),
    OJ_WIN7("KERNEL32.dll", GetStdHandle),
    OJ_WIN7("KERNEL32.dll", GetStringTypeW),
    OJ_WIN7("KERNEL32.dll", GetSystemInfo),
    OJ_WIN7("KERNEL32.dll", GetSystemTimeAsFileTime),
    OJ_WIN7("KERNEL32.dll", GetTempPathW),
    OJ_WIN7("KERNEL32.dll", GetTickCount),
    OJ_WIN7("KERNEL32.dll", GetTickCount64),
    OJ_WIN7("KERNEL32.dll", GetTimeFormatW),
    OJ_WIN7("KERNEL32.dll", GetTimeZoneInformation),
    OJ_WIN7("KERNEL32.dll", GetUserDefaultLCID),
    OJ_WIN7("KERNEL32.dll", GetUserDefaultLangID),
    OJ_WIN7("KERNEL32.dll", HeapAlloc),
    OJ_WIN7("KERNEL32.dll", HeapFree),
    OJ_WIN7("KERNEL32.dll", HeapQueryInformation),
    OJ_WIN7("KERNEL32.dll", HeapReAlloc),
    OJ_WIN7("KERNEL32.dll", HeapSize),
    OJ_WIN7("KERNEL32.dll", HeapValidate),
    OJ_WIN7("KERNEL32.dll", InitializeCriticalSectionEx),
    OJ_WIN7("KERNEL32.dll", InitializeSListHead),
    OJ_WIN7("KERNEL32.dll", InterlockedFlushSList),
    OJ_WIN7("KERNEL32.dll", InterlockedPushEntrySList),
    OJ_WIN7("KERNEL32.dll", IsDebuggerPresent),
    OJ_WIN7("KERNEL32.dll", IsProcessorFeaturePresent),
    OJ_WIN7("KERNEL32.dll", IsThreadAFiber),
    OJ_WIN7("KERNEL32.dll", IsValidCodePage),
    OJ_WIN7("KERNEL32.dll", IsValidLocale),
    OJ_WIN7("KERNEL32.dll", LCMapStringEx),
    OJ_WIN7("KERNEL32.dll", LCMapStringW),
    OJ_WIN7("KERNEL32.dll", LeaveCriticalSection),
    OJ_WIN7("KERNEL32.dll", LoadLibraryA),
    OJ_WIN7("KERNEL32.dll", LoadLibraryExW),
    OJ_WIN7("KERNEL32.dll", LoadLibraryW),
    OJ_WIN7("KERNEL32.dll", LocalFree),
    OJ_WIN7("KERNEL32.dll", MoveFileExW),
    OJ_WIN7("KERNEL32.dll", MultiByteToWideChar),
    OJ_WIN7("KERNEL32.dll", OutputDebugStringA),
    OJ_WIN7("KERNEL32.dll", OutputDebugStringW),
    OJ_WIN7("KERNEL32.dll", PeekNamedPipe),
    OJ_WIN7("KERNEL32.dll", QueryPerformanceCounter),
    OJ_WIN7("KERNEL32.dll", QueryPerformanceFrequency),
    OJ_WIN7("KERNEL32.dll", RaiseException),
    OJ_WIN7("KERNEL32.dll", ReadConsoleW),
    OJ_WIN7("KERNEL32.dll", ReadFile),
    OJ_WIN7("KERNEL32.dll", ReleaseSRWLockExclusive),
    OJ_WIN7("KERNEL32.dll", ReleaseSRWLockShared),
    OJ_WIN7("KERNEL32.dll", RemoveDirectoryW),
    OJ_WIN7("KERNEL32.dll", ResumeThread),
    OJ_WIN7("KERNEL32.dll", RtlUnwind),
    OJ_WIN7("KERNEL32.dll", SetConsoleCtrlHandler),
    OJ_WIN7("KERNEL32.dll", SetCurrentDirectoryW),
    OJ_WIN7("KERNEL32.dll", SetEndOfFile),
    OJ_WIN7("KERNEL32.dll", SetEnvironmentVariableW),
    OJ_WIN7("KERNEL32.dll", SetFileAttributesW),
    OJ_WIN7("KERNEL32.dll", SetFileInformationByHandle),
    OJ_WIN7("KERNEL32.dll", SetFilePointerEx),
    OJ_WIN7("KERNEL32.dll", SetFileTime),
    OJ_WIN7("KERNEL32.dll", SetLastError),
    OJ_WIN7("KERNEL32.dll", SetStdHandle),
    OJ_WIN7("KERNEL32.dll", SetThreadAffinityMask),
    OJ_WIN7("KERNEL32.dll", SetUnhandledExceptionFilter),
    OJ_WIN7("KERNEL32.dll", Sleep),
    OJ_WIN7("KERNEL32.dll", SleepConditionVariableSRW),
    OJ_WIN7("KERNEL32.dll", SwitchToThread),
    OJ_WIN7("KERNEL32.dll", SystemTimeToTzSpecificLocalTime),
    OJ_WIN7("KERNEL32.dll", TerminateProcess),
    OJ_WIN7("KERNEL32.dll", TryAcquireSRWLockExclusive),
    OJ_WIN7("KERNEL32.dll", TryAcquireSRWLockShared),
    OJ_WIN7("KERNEL32.dll", UnhandledExceptionFilter),
    OJ_WIN7("KERNEL32.dll", VirtualProtect),
    OJ_WIN7("KERNEL32.dll", VirtualQuery),
    OJ_WIN7("KERNEL32.dll", WaitForSingleObjectEx),
    OJ_WIN7("KERNEL32.dll", WakeAllConditionVariable),
    OJ_WIN7("KERNEL32.dll", WakeConditionVariable),
    OJ_WIN7("KERNEL32.dll", WideCharToMultiByte),
    OJ_WIN7("KERNEL32.dll", WriteConsoleW),
    OJ_WIN7("KERNEL32.dll", WriteFile),
    OJ_WIN7("KERNEL32.dll", WritePrivateProfileStringW),
#if defined(_M_X64)
    OJ_WIN7("KERNEL32.dll", RtlCaptureContext),
    OJ_WIN7("KERNEL32.dll", RtlLookupFunctionEntry),
    OJ_WIN7("KERNEL32.dll", RtlPcToFileHeader),
    OJ_WIN7("KERNEL32.dll", RtlUnwindEx),
    OJ_WIN7("KERNEL32.dll", RtlVirtualUnwind),
#endif
    // USER32.dll
    OJ_WIN7("USER32.dll", AdjustWindowRect),
    OJ_WIN7("USER32.dll", CreateWindowExA),
    OJ_WIN7("USER32.dll", DefWindowProcA),
    OJ_WIN7("USER32.dll", DestroyWindow),
    OJ_WIN7("USER32.dll", DialogBoxParamA),
    OJ_WIN7("USER32.dll", DialogBoxParamW),
    OJ_WIN7("USER32.dll", DispatchMessageA),
    OJ_WIN7("USER32.dll", EnableWindow),
    OJ_WIN7("USER32.dll", EndDialog),
    OJ_WIN7("USER32.dll", FindWindowA),
    OJ_WIN7("USER32.dll", GetActiveWindow),
    OJ_WIN7("USER32.dll", GetClientRect),
    OJ_WIN7("USER32.dll", GetCursor),
    OJ_WIN7("USER32.dll", GetDesktopWindow),
    OJ_WIN7("USER32.dll", GetDlgItem),
    OJ_WIN7("USER32.dll", GetForegroundWindow),
    OJ_WIN7("USER32.dll", GetMonitorInfoA),
    OJ_WIN7("USER32.dll", LoadCursorA),
    OJ_WIN7("USER32.dll", LoadIconA),
    OJ_WIN7("USER32.dll", LoadImageA),
    OJ_WIN7("USER32.dll", LoadStringA),
    OJ_WIN7("USER32.dll", LoadStringW),
    OJ_WIN7("USER32.dll", MessageBoxA),
    OJ_WIN7("USER32.dll", MessageBoxW),
    OJ_WIN7("USER32.dll", PeekMessageA),
    OJ_WIN7("USER32.dll", PostQuitMessage),
    OJ_WIN7("USER32.dll", RegisterClassA),
    OJ_WIN7("USER32.dll", SendMessageA),
    OJ_WIN7("USER32.dll", SendMessageW),
    OJ_WIN7("USER32.dll", SetActiveWindow),
    OJ_WIN7("USER32.dll", SetCursor),
    OJ_WIN7("USER32.dll", SetFocus),
    OJ_WIN7("USER32.dll", ShowCursor),
    OJ_WIN7("USER32.dll", ShowWindow),
    OJ_WIN7("USER32.dll", SystemParametersInfoW),
    OJ_WIN7("USER32.dll", TranslateMessage),
    OJ_WIN7("USER32.dll", UnregisterClassA),
    OJ_WIN7("USER32.dll", UpdateWindow),
    // GDI32.dll
    OJ_WIN7("GDI32.dll", GetStockObject),
    // ADVAPI32.dll
    OJ_WIN7("ADVAPI32.dll", RegCloseKey),
    OJ_WIN7("ADVAPI32.dll", RegCreateKeyExA),
    OJ_WIN7("ADVAPI32.dll", RegOpenKeyExA),
    OJ_WIN7("ADVAPI32.dll", RegQueryValueExA),
    OJ_WIN7("ADVAPI32.dll", RegSetValueExA),
    // SHELL32.dll
    OJ_WIN7("SHELL32.dll", SHGetSpecialFolderPathW),
    // ole32.dll
    OJ_WIN7("ole32.dll", CoCreateInstance),
    OJ_WIN7("ole32.dll", CoInitialize),
    OJ_WIN7("ole32.dll", CoInitializeEx),
    OJ_WIN7("ole32.dll", CoUninitialize),
    // OLEAUT32.dll
    OJ_WIN7_ORDINAL("OLEAUT32.dll", 8, VariantInit),
    OJ_WIN7_ORDINAL("OLEAUT32.dll", 9, VariantClear),
    // COMCTL32.dll
    OJ_WIN7("COMCTL32.dll", InitCommonControlsEx),
    // WS2_32.dll
    OJ_WIN7_ORDINAL("WS2_32.dll", 1, accept),
    OJ_WIN7_ORDINAL("WS2_32.dll", 2, bind),
    OJ_WIN7_ORDINAL("WS2_32.dll", 3, closesocket),
    OJ_WIN7_ORDINAL("WS2_32.dll", 4, connect),
    OJ_WIN7_ORDINAL("WS2_32.dll", 6, getsockname),
    OJ_WIN7_ORDINAL("WS2_32.dll", 7, getsockopt),
    OJ_WIN7_ORDINAL("WS2_32.dll", 8, htonl),
    OJ_WIN7_ORDINAL("WS2_32.dll", 9, htons),
    OJ_WIN7_ORDINAL("WS2_32.dll", 10, ioctlsocket),
    OJ_WIN7_ORDINAL("WS2_32.dll", 11, inet_addr),
    OJ_WIN7_ORDINAL("WS2_32.dll", 12, inet_ntoa),
    OJ_WIN7_ORDINAL("WS2_32.dll", 13, listen),
    OJ_WIN7_ORDINAL("WS2_32.dll", 14, ntohl),
    OJ_WIN7_ORDINAL("WS2_32.dll", 15, ntohs),
    OJ_WIN7_ORDINAL("WS2_32.dll", 16, recv),
    OJ_WIN7_ORDINAL("WS2_32.dll", 17, recvfrom),
    OJ_WIN7_ORDINAL("WS2_32.dll", 18, select),
    OJ_WIN7_ORDINAL("WS2_32.dll", 19, send),
    OJ_WIN7_ORDINAL("WS2_32.dll", 20, sendto),
    OJ_WIN7_ORDINAL("WS2_32.dll", 21, setsockopt),
    OJ_WIN7_ORDINAL("WS2_32.dll", 22, shutdown),
    OJ_WIN7_ORDINAL("WS2_32.dll", 23, socket),
    OJ_WIN7_ORDINAL("WS2_32.dll", 52, gethostbyname),
    OJ_WIN7_ORDINAL("WS2_32.dll", 57, gethostname),
    OJ_WIN7_ORDINAL("WS2_32.dll", 111, WSAGetLastError),
    OJ_WIN7_ORDINAL("WS2_32.dll", 115, WSAStartup),
    OJ_WIN7_ORDINAL("WS2_32.dll", 116, WSACleanup),
    OJ_WIN7_ORDINAL("WS2_32.dll", 151, __WSAFDIsSet),
    // dbghelp.dll
    OJ_WIN7("dbghelp.dll", StackWalk64),
    OJ_WIN7("dbghelp.dll", SymFromAddr),
    OJ_WIN7("dbghelp.dll", SymFunctionTableAccess64),
    OJ_WIN7("dbghelp.dll", SymGetLineFromAddr64),
    OJ_WIN7("dbghelp.dll", SymGetModuleBase64),
    OJ_WIN7("dbghelp.dll", SymInitialize),
    OJ_WIN7("dbghelp.dll", SymSetOptions),
    // d3d9.dll
    OJ_WIN7("d3d9.dll", Direct3DCreate9),
    // DINPUT8.dll
    OJ_WIN7("DINPUT8.dll", DirectInput8Create),
    // DSOUND.dll
    OJ_WIN7_ORDINAL("DSOUND.dll", 1, DirectSoundCreate),
    OJ_WIN7_ORDINAL("DSOUND.dll", 2, DirectSoundEnumerateA),
    OJ_WIN7_ORDINAL("DSOUND.dll", 6, DirectSoundCaptureCreate),
    OJ_WIN7_ORDINAL("DSOUND.dll", 7, DirectSoundCaptureEnumerateA),
    OJ_WIN7_ORDINAL("DSOUND.dll", 9, GetDeviceID),
};

#undef OJ_WIN7
#undef OJ_WIN7_ORDINAL

// An executable of the tests' configuration, beside them: the game's (OpenJuice) or the settings tool's
// (OpenJuiceConfig).
std::filesystem::path executable(const std::wstring& base) {
    wchar_t tests[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, tests, MAX_PATH);
#if defined(_M_X64)
    const std::wstring name = base + L"64";
#else
    const std::wstring name = base;
#endif
#if defined(_DEBUG)
    return std::filesystem::path(tests).parent_path() / (name + L"_d.exe");
#else
    return std::filesystem::path(tests).parent_path() / (name + L".exe");
#endif
}

// A portable executable's bytes as the system's loader reads them: the header's versions and the imports.
class PeFile {
public:
    struct Import {
        std::string dll;
        std::string name;  // empty for one by ordinal
        uint16_t ordinal = 0;
    };

    explicit PeFile(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {
        const auto* dos = at<IMAGE_DOS_HEADER>(0);
        if (dos == nullptr || dos->e_magic != IMAGE_DOS_SIGNATURE) return;
        m_nt = static_cast<size_t>(dos->e_lfanew);
        const auto* nt = at<IMAGE_NT_HEADERS32>(m_nt);
        if (nt == nullptr || nt->Signature != IMAGE_NT_SIGNATURE) return;
        m_wide = nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        if (m_wide && at<IMAGE_NT_HEADERS64>(m_nt) == nullptr) return;
        m_sections = m_nt + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + nt->FileHeader.SizeOfOptionalHeader;
        m_sectionCount = nt->FileHeader.NumberOfSections;
        m_valid = true;
    }

    bool valid() const { return m_valid; }
    bool wide() const { return m_wide; }

    // The Windows version the header asks for, major x 100 + minor: the operating system's and the subsystem's.
    int operatingSystem() const {
        return m_wide ? version(at<IMAGE_NT_HEADERS64>(m_nt)->OptionalHeader.MajorOperatingSystemVersion,
                                at<IMAGE_NT_HEADERS64>(m_nt)->OptionalHeader.MinorOperatingSystemVersion)
                      : version(at<IMAGE_NT_HEADERS32>(m_nt)->OptionalHeader.MajorOperatingSystemVersion,
                                at<IMAGE_NT_HEADERS32>(m_nt)->OptionalHeader.MinorOperatingSystemVersion);
    }
    int subsystem() const {
        return m_wide ? version(at<IMAGE_NT_HEADERS64>(m_nt)->OptionalHeader.MajorSubsystemVersion,
                                at<IMAGE_NT_HEADERS64>(m_nt)->OptionalHeader.MinorSubsystemVersion)
                      : version(at<IMAGE_NT_HEADERS32>(m_nt)->OptionalHeader.MajorSubsystemVersion,
                                at<IMAGE_NT_HEADERS32>(m_nt)->OptionalHeader.MinorSubsystemVersion);
    }

    std::vector<Import> imports() const {
        std::vector<Import> out;
        const IMAGE_DATA_DIRECTORY directory =
            m_wide ? at<IMAGE_NT_HEADERS64>(m_nt)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                   : at<IMAGE_NT_HEADERS32>(m_nt)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        const std::optional<size_t> first = offsetOf(directory.VirtualAddress);
        if (!first) return out;
        for (size_t d = *first;; d += sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
            const auto* descriptor = at<IMAGE_IMPORT_DESCRIPTOR>(d);
            if (descriptor == nullptr || descriptor->Name == 0) break;
            const std::string dll = text(descriptor->Name);
            const uint32_t thunks =
                descriptor->OriginalFirstThunk != 0 ? descriptor->OriginalFirstThunk : descriptor->FirstThunk;
            const size_t step = m_wide ? 8 : 4;
            const std::optional<size_t> table = offsetOf(thunks);
            for (size_t t = table.value_or(m_bytes.size()); t + step <= m_bytes.size(); t += step) {
                uint64_t value = 0;
                std::memcpy(&value, m_bytes.data() + t, step);
                if (value == 0) break;
                const uint64_t byOrdinal = m_wide ? IMAGE_ORDINAL_FLAG64 : IMAGE_ORDINAL_FLAG32;
                if ((value & byOrdinal) != 0) {
                    out.push_back({dll, std::string(), static_cast<uint16_t>(value & 0xFFFF)});
                } else {
                    out.push_back({dll, text(static_cast<uint32_t>(value & 0x7FFFFFFF) + 2), 0});  // past the hint
                }
            }
        }
        return out;
    }

private:
    static int version(WORD major, WORD minor) { return major * 100 + minor; }

    template <class T>
    const T* at(size_t offset) const {
        return offset + sizeof(T) <= m_bytes.size() ? reinterpret_cast<const T*>(m_bytes.data() + offset) : nullptr;
    }

    // A relative virtual address's place in the file, by the section holding it.
    std::optional<size_t> offsetOf(uint32_t rva) const {
        for (size_t i = 0; i < m_sectionCount; ++i) {
            const auto* s = at<IMAGE_SECTION_HEADER>(m_sections + i * sizeof(IMAGE_SECTION_HEADER));
            if (s == nullptr) break;
            const uint32_t size = std::max<uint32_t>(s->Misc.VirtualSize, s->SizeOfRawData);
            if (rva >= s->VirtualAddress && rva < s->VirtualAddress + size) {
                return static_cast<size_t>(rva - s->VirtualAddress + s->PointerToRawData);
            }
        }
        return std::nullopt;
    }

    std::string text(uint32_t rva) const {
        std::string out;
        const std::optional<size_t> start = offsetOf(rva);
        for (size_t o = start.value_or(m_bytes.size()); o < m_bytes.size() && m_bytes[o] != 0; ++o) {
            out.push_back(static_cast<char>(m_bytes[o]));
        }
        return out;
    }

    std::vector<uint8_t> m_bytes;
    size_t m_nt = 0;
    size_t m_sections = 0;
    size_t m_sectionCount = 0;
    bool m_wide = false;
    bool m_valid = false;
};

bool onWindows7(const PeFile::Import& import) {
    for (const Windows7Function& f : kWindows7) {
        if (_stricmp(f.dll, import.dll.c_str()) != 0) continue;
        if (import.name.empty() ? f.ordinal == import.ordinal : import.name == f.name) return true;
    }
    return false;
}

// A file of the system's temporary folder, of this run's own.
std::wstring temporaryFile(const wchar_t* tag) {
    wchar_t folder[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, folder);
    return std::wstring(folder) + L"OpenJuice.Windows7." + tag + L"." + std::to_wstring(GetCurrentProcessId());
}

uint64_t ticks(const FILETIME& t) {
    return (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
}

// The executable starts on Windows 7: its header asks for 6.1 at most, and it imports only functions Windows 7 has
// (at least the count given read).
void checkStartsOnWindows7(const std::filesystem::path& file, size_t leastImports) {
    std::ifstream in(file, std::ios::binary);
    check(in.good(), "the executable beside the tests: " + file.filename().string());
    if (!in) return;
    const PeFile pe(std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}));
    check(pe.valid(), "the executable reads as a portable executable");
    if (!pe.valid()) return;
#if defined(_M_X64)
    check(pe.wide(), "the 64-bit tests' executable is a 64-bit one");
#else
    check(!pe.wide(), "the 32-bit tests' executable is a 32-bit one");
#endif
    check(pe.operatingSystem() <= 601 && pe.subsystem() <= 601,
          "the header asks for Windows 6.1 at most (the system " + std::to_string(pe.operatingSystem()) +
              ", the subsystem " + std::to_string(pe.subsystem()) + ")");
    const std::vector<PeFile::Import> imports = pe.imports();
    check(imports.size() >= leastImports, "the imports read (" + std::to_string(imports.size()) + ")");
    std::string later;
    for (const PeFile::Import& import : imports) {
        if (onWindows7(import)) continue;
        later += " " + import.dll + "!" + (import.name.empty() ? "#" + std::to_string(import.ordinal) : import.name);
    }
    check(later.empty(), "every import is one of Windows 7's" + (later.empty() ? std::string() : ", not:" + later));
}

}  // namespace

// The game's executable starts on Windows 7.
OJ_INTEGRATION_TEST(Windows7, Executable) { checkStartsOnWindows7(executable(L"OpenJuice"), 100); }

// The settings tool's executable starts on Windows 7.
OJ_INTEGRATION_TEST(Windows7, ConfigTool) { checkStartsOnWindows7(executable(L"OpenJuiceConfig"), 50); }

// CreateFile2 without Windows 8: CreateFileW with its parameters' attributes and flags.
OJ_INTEGRATION_TEST(Windows7, CreateFile2) {
    const std::wstring name = temporaryFile(L"create");
    oj::win7::CreateFile2Parameters parameters;
    parameters.attributes = FILE_ATTRIBUTE_TEMPORARY;
    parameters.flags = FILE_FLAG_DELETE_ON_CLOSE;
    const HANDLE file = oj::win7::createFile2(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, &parameters);
    check(file != INVALID_HANDLE_VALUE, "a new file made");
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    check(WriteFile(file, "juice", 5, &written, nullptr) && written == 5, "the file written");
    const DWORD attributes = GetFileAttributesW(name.c_str());
    check(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_TEMPORARY) != 0,
          "the parameters' attribute given");
    CloseHandle(file);
    check(GetFileAttributesW(name.c_str()) == INVALID_FILE_ATTRIBUTES, "the parameters' flag given: deleted on close");

    SetLastError(0);
    const HANDLE missing = oj::win7::createFile2(name.c_str(), GENERIC_READ, 0, OPEN_EXISTING, nullptr);
    check(missing == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND,
          "a missing file: INVALID_HANDLE_VALUE, ERROR_FILE_NOT_FOUND");
}

// CopyFile2 without Windows 8: CopyFileExW with the flags Windows 7 knows, its result as an HRESULT.
OJ_INTEGRATION_TEST(Windows7, CopyFile2) {
    const std::wstring from = temporaryFile(L"from");
    const std::wstring to = temporaryFile(L"to");
    {
        std::ofstream out(std::filesystem::path(from), std::ios::binary);
        out << "juiced";
    }
    DeleteFileW(to.c_str());
    oj::win7::CopyFile2Parameters parameters;
    parameters.copyFlags = COPY_FILE_FAIL_IF_EXISTS;
    check(oj::win7::copyFile2(from.c_str(), to.c_str(), &parameters) == S_OK, "the file copied");
    std::string bytes;
    {
        std::ifstream copied(std::filesystem::path(to), std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(copied), {});
    }
    check(bytes == "juiced", "the copy holds the file's bytes");
    check(oj::win7::copyFile2(from.c_str(), to.c_str(), &parameters) == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS),
          "COPY_FILE_FAIL_IF_EXISTS over a copy: HRESULT_FROM_WIN32(ERROR_FILE_EXISTS)");
    // COPY_FILE_NO_OFFLOAD (0x40000) is CopyFile2's only: left out, the copy made over the old one.
    parameters.copyFlags = 0x00040000;
    check(oj::win7::copyFile2(from.c_str(), to.c_str(), &parameters) == S_OK, "a flag of CopyFile2's only left out");
    DeleteFileW(from.c_str());
    DeleteFileW(to.c_str());
}

// GetSystemTimePreciseAsFileTime without Windows 8: the system's time of its last tick.
OJ_INTEGRATION_TEST(Windows7, SystemTimePrecise) {
    FILETIME before{}, now{}, after{};
    GetSystemTimeAsFileTime(&before);
    oj::win7::systemTimePrecise(&now);
    GetSystemTimeAsFileTime(&after);
    check(ticks(before) <= ticks(now) && ticks(now) <= ticks(after), "the time between the system's two readings");
}
