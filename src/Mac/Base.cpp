#include "Core/Base/Config.h"
#include "Core/Base/Cpu.h"
#include "Core/Base/CpuInfo.h"
#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <codecvt>
#include <cstdarg>
#include <fstream>
#include <locale>
#include <mutex>
namespace oj {
std::string toUtf8(std::wstring_view s) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(s.data(), s.data() + s.size());
}
std::wstring toWide(std::string_view s) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(s.data(), s.data() + s.size());
}
std::wstring widenSigned(std::string_view s) {
    std::wstring o;
    for (char c : s) {
        if (!c)
            break;
        o.push_back((uint16_t)(int16_t)(int8_t)c);
    }
    return o;
}
Config Config::load(const std::filesystem::path &path) {
    Config c;
    std::ifstream in(path);
    std::string line, section;
    if (!in) {
        c.save(path);
        return c;
    }
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.starts_with('[')) {
            section = line;
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        auto key = line.substr(0, eq), value = line.substr(eq + 1);
        if (section == "[Game]") {
            if (key == "Language")
                c.language = value;
            if (key == "SkipIntroMovies")
                c.skipIntroMovies = value == "1";
        } else if (section == "[Saves]" && key == "Folder")
            c.saveFolder = value;
    }
    return c;
}
void Config::save(const std::filesystem::path &path) const {
    std::ofstream out(path);
    out << "[Game]\nLanguage=" << language << "\nSkipIntroMovies=" << skipIntroMovies
        << "\n[Saves]\nFolder=" << saveFolder.string() << "\n";
}
void cpuName(const char *vendor, uint32_t, uint32_t, std::array<char, 31> &out) {
    out.fill(0);
    std::strncpy(out.data(), vendor, 30);
}
uint32_t queryCpu(CpuInfo *info) {
    if (info) {
        *info = {};
#ifdef OJ_ANDROID
        cpuName("Android ARM64", 0, 0, info->name);
#else
        cpuName("Apple Silicon", 0, 0, info->name);
#endif
    }
    return 0;
}
namespace cpu {
uint32_t detectFeatures() {
    return 0;
}
} // namespace cpu
} // namespace oj
namespace oj::log {
static std::mutex lock;
static std::ofstream file;
void open(const std::filesystem::path &p) {
    std::lock_guard g(lock);
    file.open(p);
}
void close() {
    std::lock_guard g(lock);
    file.close();
}
void write(Level level, std::string_view text) {
    std::lock_guard g(lock);
    auto tag = level == Level::Error ? "error: " : level == Level::Warning ? "warning: " : "";
    fprintf(stderr, "%s%.*s\n", tag, (int)text.size(), text.data());
    if (file) {
        file << tag << text << '\n';
        file.flush();
    }
}
void debugPrint(const char *fmt, ...) {
    char b[4096];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    std::lock_guard g(lock);
    if (file)
        file << b;
}
void debugger(std::string_view s) {
    debugPrint("%.*s\n", (int)s.size(), s.data());
}
} // namespace oj::log
