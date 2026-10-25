// Unit tests: the fixed build's 8-bit names (Core/Base/Text.h).

#include "Framework/Test.h"

#include "Core/Base/Text.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// The stock widens each byte with its sign (0xF3 becomes 0xFFF3, no letter); the fixed build reads the names of the
// game's files in the language's code page, up to a 0.
OJ_UNIT_TEST(NamesCodePage, ReadInTheLanguagesCodePage) {
    check(oj::namesCodePage(0) == 1252 && oj::namesCodePage(5) == 1250 && oj::namesCodePage(7) == 1251,
          "names (fixed): 1252 for English, 1250 for Czech, 1251 for Russian");
    oj::setNamesCodePage(1252);
    check(oj::widenSigned(std::string("Cimarr\xF3n\0rest", 12)) == L"Cimarrón",
          "names (fixed): Windows-1252's 0xF3 as o acute, up to the 0");
    oj::setNamesCodePage(1251);
    check(oj::widenSigned("\xC8\xE2\xE0\xED") == L"Иван", "names (fixed): Windows-1251 as Cyrillic");
    oj::setNamesCodePage(1252);
    check(oj::widenAnsi("Speakers") == L"Speakers", "names (fixed): a device's ASCII description");
}
