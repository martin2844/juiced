#pragma once
#include "windows.h"
#define CSIDL_PERSONAL 5
#define CSIDL_APPDATA 26
inline BOOL SHGetSpecialFolderPathW(HWND, wchar_t *out, int kind, BOOL) {
#ifdef OJ_ANDROID
    const char *home = getenv("JUICED_SUPPORT_DIR");
#else
    const char *home = getenv("HOME");
#endif
    if (!home)
        return 0;
#ifdef OJ_ANDROID
    auto p = std::filesystem::path(home);
#else
    auto p = std::filesystem::path(home) / (kind == CSIDL_PERSONAL ? "Documents" : "Library/Application Support");
#endif
    auto w = p.wstring();
    std::copy(w.begin(), w.end(), out);
    out[w.size()] = 0;
    return 1;
}
