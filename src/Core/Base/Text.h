#pragma once
#include <cstdint>
// Conversions between the UTF-16 of the Windows API and the UTF-8 used for text inside the engine.

#include <string>
#include <string_view>

namespace oj {

std::string toUtf8(std::wstring_view text);
std::wstring toWide(std::string_view utf8);

// The stock game's 8-bit names made wide a character at a time, up to a 0 or the end (0x0040ACE0, 0x0049A850).
// Stock problem: the characters are signed, so a letter above 0x7F becomes 0xFF80 .. 0xFFFF. The fixed build reads
// the bytes in the names' code page (setNamesCodePage).
std::wstring widenSigned(std::string_view text);

// The fixed build: the code page the 8-bit names of the game's files are read in, the game's language's
// (namesCodePage): Windows-1252 for English, French, German, Italian and Spanish, 1250 for Czech and Polish, 1251 for
// Russian; 1252 until set.
void setNamesCodePage(uint32_t codePage);
uint32_t namesCodePage(int languageIndex);
// The fixed build: an 8-bit text of a Windows ANSI call (a sound device's description) in the system's code page,
// up to a 0 or the end.
std::wstring widenAnsi(std::string_view text);

}  // namespace oj
