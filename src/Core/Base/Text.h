#pragma once
// Conversions between the UTF-16 of the Windows API and the UTF-8 used for text inside the engine.

#include <string>
#include <string_view>

namespace oj {

std::string toUtf8(std::wstring_view text);
std::wstring toWide(std::string_view utf8);

// The stock game's 8-bit names made wide a character at a time, up to a 0 or the end (0x0040ACE0, 0x0049A850).
// Stock problem: the characters are signed, so a letter above 0x7F becomes 0xFF80 .. 0xFFFF.
std::wstring widenSigned(std::string_view text);

}  // namespace oj
