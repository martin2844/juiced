#pragma once
// Small native adapters for platform calls still shared with the upstream engine.
// This is not a Windows runtime: files, clocks, and threads use macOS/POSIX.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
using DWORD = uint32_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using BOOL = int;
using UINT = unsigned;
using LONG = int32_t;
using ULONG = uint32_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
using HRESULT = int32_t;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LPVOID = void *;
struct HWND__;
using HWND = HWND__ *;
struct HINSTANCE__;
using HMODULE = HINSTANCE__ *;
using HINSTANCE = HMODULE;
struct NativeHandle;
using HANDLE = NativeHandle *;
#define WINAPI
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define MAX_PATH 4096
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_ATTRIBUTE_NORMAL 128
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define CREATE_NEW 1
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_FILE_SIZE 0xffffffffu
#define INVALID_FILE_ATTRIBUTES 0xffffffffu
#define ERROR_NO_MORE_FILES 18
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_SUCCESS 0
#define INFINITE 0xffffffffu
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define S_OK 0
#define E_FAIL ((HRESULT) - 1)
#define FAILED(x) ((x) < 0)
#define SUCCEEDED(x) ((x) >= 0)
#define MB_OK 0
#define MB_ICONERROR 0
#define MB_ICONHAND 0
#define MB_SETFOREGROUND 0
#define MB_TOPMOST 0
#define CP_UTF8 65001
#define CP_ACP 0
#define LOWORD(x) ((uint16_t)(x))
#define HIWORD(x) ((uint16_t)((x) >> 16))
#define MAKELONG(a, b) ((uint32_t)(a) | ((uint32_t)(b) << 16))
#define MAKEWORD(a, b) ((uint16_t)((a) | ((b) << 8)))
struct _GUID {
    uint32_t Data1;
    uint16_t Data2, Data3;
    uint8_t Data4[8];
};
using GUID = _GUID;
struct LARGE_INTEGER {
    int64_t QuadPart = 0;
};
struct FILETIME {
    DWORD dwLowDateTime = 0, dwHighDateTime = 0;
};
struct SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
};
struct WIN32_FIND_DATAW {
    DWORD dwFileAttributes = 0;
    FILETIME ftCreationTime{}, ftLastAccessTime{}, ftLastWriteTime{};
    DWORD nFileSizeHigh = 0, nFileSizeLow = 0;
    wchar_t cFileName[MAX_PATH]{};
};
using CRITICAL_SECTION = std::recursive_mutex;
inline void InitializeCriticalSection(CRITICAL_SECTION *) {}
inline void DeleteCriticalSection(CRITICAL_SECTION *) {}
inline void EnterCriticalSection(CRITICAL_SECTION *m) {
    m->lock();
}
inline void LeaveCriticalSection(CRITICAL_SECTION *m) {
    m->unlock();
}
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *v) {
    v->QuadPart = 1000000000;
    return 1;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER *v) {
    v->QuadPart =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    return 1;
}
inline DWORD GetTickCount() {
    return (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
inline void Sleep(DWORD ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
void GetLocalTime(SYSTEMTIME *t);
inline void OutputDebugStringA(const char *s) {
    fputs(s, stderr);
}
std::string nativePath(const std::filesystem::path &p);
HANDLE CreateFileW(const std::filesystem::path &, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
inline HANDLE CreateFileA(const char *p, DWORD a, DWORD b, void *c, DWORD d, DWORD e, HANDLE f) {
    return CreateFileW(p, a, b, c, d, e, f);
}
BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
BOOL CloseHandle(HANDLE);
DWORD GetFileSize(HANDLE, DWORD *);
DWORD SetFilePointer(HANDLE, LONG, LONG *, DWORD);
HANDLE FindFirstFileW(const std::filesystem::path &, WIN32_FIND_DATAW *);
BOOL FindNextFileW(HANDLE, WIN32_FIND_DATAW *);
BOOL FindClose(HANDLE);
DWORD GetLastError();
BOOL CreateDirectoryW(const std::filesystem::path &, void *);
BOOL DeleteFileW(const std::filesystem::path &);
BOOL RemoveDirectoryW(const std::filesystem::path &);
DWORD GetFileAttributesW(const std::filesystem::path &);
inline DWORD GetFileAttributesA(const char *p) {
    return GetFileAttributesW(p);
}
inline BOOL DeleteFileA(const char *p) {
    return DeleteFileW(p);
}
BOOL GetDiskFreeSpaceExW(const std::filesystem::path &, void *, void *, void *);
int MessageBoxW(HWND, const wchar_t *, const wchar_t *, UINT);
inline WORD GetUserDefaultLangID() {
    return 0x409;
}
inline int _wfopen_s(FILE **f, const std::filesystem::path &p, const wchar_t *mode) {
    std::wstring w(mode);
    std::string m(w.begin(), w.end());
    *f = fopen(nativePath(p).c_str(), m.c_str());
    return *f ? 0 : errno;
}
struct RECT {
    LONG left = 0, top = 0, right = 0, bottom = 0;
};
BOOL GetClientRect(HWND, RECT *);
DWORD GetModuleFileNameW(HMODULE, wchar_t *, DWORD);
inline DWORD GetCurrentDirectoryA(DWORD n, char *out) {
    auto s = std::filesystem::current_path().string();
    if (s.size() + 1 > n)
        return s.size() + 1;
    memcpy(out, s.c_str(), s.size() + 1);
    return s.size();
}

void PostQuitMessage(int);
BOOL SetFilePointerEx(HANDLE, LARGE_INTEGER, LARGE_INTEGER *, DWORD);
BOOL GetFileSizeEx(HANDLE, LARGE_INTEGER *);
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
