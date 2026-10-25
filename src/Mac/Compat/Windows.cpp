#ifdef OJ_LINUX
#include "Core/Base/CasePath.h"
#endif
#include "windows.h"
#include <SDL3/SDL.h>
#include <codecvt>
#include <fcntl.h>
#include <locale>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
struct NativeHandle {
    int fd = -1;
    std::vector<std::filesystem::path> entries;
    size_t next = 0;
};
static thread_local DWORD lastError = 0;
std::string nativePath(const std::filesystem::path &p) {
#ifdef OJ_LINUX
    return oj::gameNativePath(p).string();
#else
    auto s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
#endif
}
DWORD GetLastError() {
    return lastError;
}
void GetLocalTime(SYSTEMTIME *t) {
    auto now = std::chrono::system_clock::now();
    auto raw = std::chrono::system_clock::to_time_t(now);
    tm v{};
    localtime_r(&raw, &v);
    *t = {(WORD)(v.tm_year + 1900),
          (WORD)(v.tm_mon + 1),
          (WORD)v.tm_wday,
          (WORD)v.tm_mday,
          (WORD)v.tm_hour,
          (WORD)v.tm_min,
          (WORD)v.tm_sec,
          (WORD)(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000)};
}
HANDLE CreateFileW(const std::filesystem::path &p, DWORD access, DWORD, void *, DWORD how, DWORD, HANDLE) {
    int flags = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
    if (how == CREATE_NEW)
        flags |= O_CREAT | O_EXCL;
    if (how == CREATE_ALWAYS)
        flags |= O_CREAT | O_TRUNC;
    int fd = open(nativePath(p).c_str(), flags, 0600);
    if (fd < 0) {
        lastError = errno;
        return INVALID_HANDLE_VALUE;
    }
    auto h = new NativeHandle;
    h->fd = fd;
    return h;
}
BOOL CloseHandle(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE)
        return 0;
    if (h->fd >= 0)
        close(h->fd);
    delete h;
    return 1;
}
BOOL ReadFile(HANDLE h, void *b, DWORD n, DWORD *got, void *) {
    auto r = read(h->fd, b, n);
    if (got)
        *got = r < 0 ? 0 : r;
    return r >= 0;
}
BOOL WriteFile(HANDLE h, const void *b, DWORD n, DWORD *done, void *) {
    size_t total = 0;
    while (total < n) {
        auto r = write(h->fd, (const char *)b + total, n - total);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (!r)
            break;
        total += r;
    }
    if (done)
        *done = total;
    return total == n;
}
DWORD GetFileSize(HANDLE h, DWORD *high) {
    struct stat s{};
    if (fstat(h->fd, &s))
        return INVALID_FILE_SIZE;
    if (high)
        *high = (uint64_t)s.st_size >> 32;
    return s.st_size;
}
DWORD SetFilePointer(HANDLE h, LONG off, LONG *high, DWORD origin) {
    int64_t delta = off;
    if (high)
        delta = ((int64_t)*high << 32) | (uint32_t)off;
    auto r = lseek(h->fd, delta, origin);
    if (high)
        *high = (uint64_t)r >> 32;
    return r;
}
static void fill(const std::filesystem::path &p, WIN32_FIND_DATAW *d) {
    *d = {};
    struct stat s{};
    stat(p.c_str(), &s);
    d->dwFileAttributes = S_ISDIR(s.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    uint64_t stamp = ((uint64_t)s.st_mtime + 11644473600ull) * 10000000ull;
    d->ftLastWriteTime = {(DWORD)stamp, (DWORD)(stamp >> 32)};
    d->nFileSizeLow = s.st_size;
    d->nFileSizeHigh = (uint64_t)s.st_size >> 32;
    auto name = p.filename().wstring();
    wcsncpy(d->cFileName, name.c_str(), MAX_PATH - 1);
}
HANDLE FindFirstFileW(const std::filesystem::path &path, WIN32_FIND_DATAW *d) {
    std::filesystem::path p = nativePath(path);
    auto h = new NativeHandle;
    std::error_code ec;
    if (p.filename() == "*") {
        for (auto &e : std::filesystem::directory_iterator(p.parent_path(), ec))
            h->entries.push_back(e.path());
        std::sort(h->entries.begin(), h->entries.end());
    } else if (std::filesystem::exists(p, ec))
        h->entries.push_back(p);
    if (h->entries.empty()) {
        delete h;
        lastError = ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    fill(h->entries[h->next++], d);
    lastError = 0;
    return h;
}
BOOL FindNextFileW(HANDLE h, WIN32_FIND_DATAW *d) {
    if (h->next >= h->entries.size()) {
        lastError = ERROR_NO_MORE_FILES;
        return 0;
    }
    fill(h->entries[h->next++], d);
    return 1;
}
BOOL FindClose(HANDLE h) {
    return CloseHandle(h);
}
BOOL CreateDirectoryW(const std::filesystem::path &p, void *) {
    std::error_code ec;
    return std::filesystem::create_directory(nativePath(p), ec);
}
BOOL DeleteFileW(const std::filesystem::path &p) {
    return unlink(nativePath(p).c_str()) == 0;
}
BOOL RemoveDirectoryW(const std::filesystem::path &p) {
    return rmdir(nativePath(p).c_str()) == 0;
}
DWORD GetFileAttributesW(const std::filesystem::path &p) {
    std::error_code ec;
    auto s = std::filesystem::status(nativePath(p), ec);
    if (ec || !std::filesystem::exists(s))
        return INVALID_FILE_ATTRIBUTES;
    return std::filesystem::is_directory(s) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
BOOL GetDiskFreeSpaceExW(const std::filesystem::path &p, void *a, void *t, void *f) {
    std::error_code ec;
    auto s = std::filesystem::space(nativePath(p), ec);
    if (ec)
        return 0;
    if (a)
        *(uint64_t *)a = s.available;
    if (t)
        *(uint64_t *)t = s.capacity;
    if (f)
        *(uint64_t *)f = s.free;
    return 1;
}
int MessageBoxW(HWND, const wchar_t *text, const wchar_t *title, UINT) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, cv.to_bytes(title).c_str(), cv.to_bytes(text).c_str(), nullptr);
    return 1;
}
BOOL GetClientRect(HWND h, RECT *r) {
    int w = 0, height = 0;
    bool ok = SDL_GetWindowSizeInPixels(reinterpret_cast<SDL_Window *>(h), &w, &height);
    *r = {0, 0, w, height};
    return ok;
}
DWORD GetModuleFileNameW(HMODULE, wchar_t *out, DWORD n) {
    auto path = std::filesystem::path(SDL_GetBasePath()) / "juiced";
    auto w = path.wstring();
    if (w.size() >= n)
        return 0;
    std::copy(w.begin(), w.end(), out);
    out[w.size()] = 0;
    return w.size();
}

void PostQuitMessage(int) {
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}
BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER offset, LARGE_INTEGER *out, DWORD origin) {
    auto r = lseek(h->fd, offset.QuadPart, origin);
    if (out)
        out->QuadPart = r;
    return r >= 0;
}
BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER *out) {
    struct stat s{};
    if (fstat(h->fd, &s))
        return 0;
    out->QuadPart = s.st_size;
    return 1;
}
