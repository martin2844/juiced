// Unit tests: the game's language (Game/Platform/Platform.h chooseLanguage), no region bound to it.

#include "Framework/Test.h"

#include "Core/Data/Language.h"
#include "Game/Platform/Platform.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// The settings' language id as the English build 1.0.1 takes it, the three languages more; [Game] Language over it.
OJ_UNIT_TEST(LanguageChoice, BySettingsAndIni) {
    const auto name = [](int32_t id, const std::string& ini) { return std::string(oj::chooseLanguage(id, ini).name); };
    check(name(0x0407, "") == "german" && name(0x040C, "") == "french" && name(0x0410, "") == "italian" &&
              name(0x0C0A, "") == "spanish" && name(0x0409, "") == "english",
          "language: German, French, Italian, Spanish, English by the id");
    check(name(0x0405, "") == "czech" && name(0x0415, "") == "polish" && name(0x0419, "") == "russian",
          "language: Czech, Polish and Russian by the id too");
    check(name(0x0411, "") == "english", "language: another id English");
    check(name(0x0407, "polish") == "polish" && name(0x0407, "klingon") == "german",
          "language: [Game] Language names one; an unknown name is passed over");
    const oj::Language none;
    check(none.index == 0 && none.controlsLetter == 'e', "language: English when none is picked");
}
