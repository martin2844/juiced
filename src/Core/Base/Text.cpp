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

std::wstring widenSigned(std::string_view text) {
    std::wstring out;
    for (char c : text) {
        if (c == '\0') break;
        out.push_back(static_cast<wchar_t>(static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c)))));
    }
    return out;
}

}  // namespace oj
