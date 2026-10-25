#include "Core/Base/Text.h"

#include <windows.h>

#include <cstdint>

namespace oj {

std::string toUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
    return out;
}

namespace {

uint32_t g_namesCodePage = 1252;

std::wstring widenIn(uint32_t codePage, std::string_view text) {
    text = text.substr(0, text.find('\0'));
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(codePage, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(codePage, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

}  // namespace

std::wstring widenSigned(std::string_view text) {
    return widenIn(g_namesCodePage, text);
}

void setNamesCodePage(uint32_t codePage) {
    g_namesCodePage = codePage;
}

uint32_t namesCodePage(int languageIndex) {
    switch (languageIndex) {
    case 5:  // Czech
    case 6:  // Polish
        return 1250;
    case 7:  // Russian
        return 1251;
    default:
        return 1252;
    }
}

std::wstring widenAnsi(std::string_view text) {
    return widenIn(CP_ACP, text);
}

}  // namespace oj
