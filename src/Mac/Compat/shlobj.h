#pragma once
#include "windows.h"
#ifdef OJ_LINUX
#include "Linux/Paths.h"
#endif
#define CSIDL_PERSONAL 5
#define CSIDL_APPDATA 26
inline BOOL SHGetSpecialFolderPathW(HWND, wchar_t *out, int kind, BOOL) {
    const char *home = getenv("HOME");
    if (!home)
        return 0;
#ifdef OJ_LINUX
    auto p = kind == CSIDL_PERSONAL ? std::filesystem::path(home) / "Documents"
                                   : oj::linux_port::dataPath().parent_path();
#else
    auto p = std::filesystem::path(home) / (kind == CSIDL_PERSONAL ? "Documents" : "Library/Application Support");
#endif
    auto w = p.wstring();
    std::copy(w.begin(), w.end(), out);
    out[w.size()] = 0;
    return 1;
}
