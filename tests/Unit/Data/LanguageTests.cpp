// Unit tests: Language.

#include "Framework/Test.h"

#include "Core/Data/Language.h"

namespace {

using oj::test::check;

}  // namespace

// The language by a Windows language id (1.0.1's five and the three more) and the graphics class of an adapter.
OJ_UNIT_TEST(Language, LanguageOfLangId) {
    check(oj::languageOfLangId(0x0409) == 0 && oj::languageOfLangId(0x040C) == 1 && oj::languageOfLangId(0x0407) == 2 &&
              oj::languageOfLangId(0x0410) == 3 && oj::languageOfLangId(0x0C0A) == 4 &&
              oj::languageOfLangId(0x0405) == 5 && oj::languageOfLangId(0x0415) == 6 &&
              oj::languageOfLangId(0x0419) == 7 && oj::languageOfLangId(0x0411) == 0 && oj::languageOfLangId(9) == 0,
          "language by the language id");
}
