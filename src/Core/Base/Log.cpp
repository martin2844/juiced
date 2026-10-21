#include "Core/Base/Log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace oj::log {

namespace {

std::mutex g_mutex;
FILE* g_file = nullptr;

const char* prefix(Level level) {
    switch (level) {
    case Level::Warning: return "warning: ";
    case Level::Error: return "error: ";
    default: return "";
    }
}

}  // namespace

void open(const std::filesystem::path& file) {
    std::lock_guard lock(g_mutex);
    if (g_file != nullptr) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    _wfopen_s(&g_file, file.c_str(), L"wb");
}

void close() {
    std::lock_guard lock(g_mutex);
    if (g_file != nullptr) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void write(Level level, std::string_view text) {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    const std::string line = std::format("{:02}:{:02}:{:02}.{:03} {}{}\n", t.wHour, t.wMinute, t.wSecond,
                                         t.wMilliseconds, prefix(level), text);
    std::lock_guard lock(g_mutex);
    OutputDebugStringA(line.c_str());
    if (g_file != nullptr) {
        std::fwrite(line.data(), 1, line.size(), g_file);
        std::fflush(g_file);
    }
}

void debugPrint(const char* format, ...) {
    // The stock buffer has no bound (vsprintf); here 4096 characters.
    static char buffer[4096];
    std::lock_guard lock(g_mutex);
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

void debugger(std::string_view text) {
    const std::string line(text);
    OutputDebugStringA(line.c_str());
    OutputDebugStringA("\r\n");
}

}  // namespace oj::log
