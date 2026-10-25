#include "Core/Base/Config.h"
#include "Core/Base/Cpu.h"
#include "Core/Base/CpuInfo.h"
#include "Core/Base/Log.h"
#include "Core/Base/Text.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <iconv.h>
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
namespace { uint32_t namesPage = 1252; }
void setNamesCodePage(uint32_t page) { namesPage = page; }
uint32_t namesCodePage(int language) { return language == 7 ? 1251 : (language == 5 || language == 6) ? 1250 : 1252; }
static std::wstring widenPage(std::string_view text, uint32_t page) {
    text = text.substr(0, text.find('\0'));
    const auto from = "CP" + std::to_string(page);
    iconv_t converter = iconv_open("WCHAR_T", from.c_str());
    if (converter == (iconv_t)-1) return {};
    std::wstring result(text.size(), L'\0');
    char* input = const_cast<char*>(text.data());
    char* output = reinterpret_cast<char*>(result.data());
    size_t left = text.size(), available = result.size() * sizeof(wchar_t);
    while (left) {
        if (iconv(converter, &input, &left, &output, &available) != size_t(-1)) break;
        // Undefined code-page bytes become the replacement character, never a hang.
        if (available < sizeof(wchar_t)) break;
        *reinterpret_cast<wchar_t*>(output) = L'\uFFFD';
        output += sizeof(wchar_t); available -= sizeof(wchar_t); ++input; --left;
    }
    iconv_close(converter);
    result.resize((output - reinterpret_cast<char*>(result.data())) / sizeof(wchar_t));
    return result;
}
std::wstring widenSigned(std::string_view text) { return widenPage(text, namesPage); }
std::wstring widenAnsi(std::string_view text) { return widenPage(text, 1252); }
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
#ifdef OJ_LINUX
        cpuName("Native Linux", 0, 0, info->name);
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
