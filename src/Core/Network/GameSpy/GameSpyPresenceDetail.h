#pragma once
// The SDK's string helpers shared by GameSpy Presence's parts (Core/Network/GameSpy/GameSpyPresence*.cpp).

#include <string>
#include <string_view>

namespace oj::net::gp::detail {

// The narrow strings of the wide calls: each character's low byte.
std::string narrow(std::u16string_view text);
// 0x00568E10: each byte zero-extended.
std::u16string widenBytes(std::string_view text);
// strncpy into size bytes with the last made 0: at most size - 1 characters.
std::string cut(std::string_view text, size_t size);
// The C string of the bytes: up to the first 0.
std::string_view cString(const std::string& text);
// gpiValueForKey (0x00579AF0): the key anywhere in the message (strstr), its value up to the next character equal to
// the key's first (the delimiter) or the end, at most size - 1 characters; false without the key.
bool valueForKey(std::string_view message, std::string_view key, std::string& out, size_t size);
long atolOf(const std::string& text);
// '\' made '/'.
std::string slashes(std::string text);
// 0x005799B0: UTF-8 into a buffer of size wide characters (at most size - 1 of them).
std::u16string fromUtf8Cut(std::string_view text, size_t size);

}  // namespace oj::net::gp::detail
