// Unit tests: Language.

#include "Framework/Test.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/Language.h"

#include <filesystem>
#include <fstream>

namespace {

using oj::test::check;

// A file of the folder (its folders made).
void touch(const std::filesystem::path& file) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "x";
}

}  // namespace

// The language by a Windows language id (1.0.1's five and the three more) and the graphics class of an adapter.
OJ_UNIT_TEST(Language, LanguageOfLangId) {
    check(oj::languageOfLangId(0x0409) == 0 && oj::languageOfLangId(0x040C) == 1 && oj::languageOfLangId(0x0407) == 2 &&
              oj::languageOfLangId(0x0410) == 3 && oj::languageOfLangId(0x0C0A) == 4 &&
              oj::languageOfLangId(0x0405) == 5 && oj::languageOfLangId(0x0415) == 6 &&
              oj::languageOfLangId(0x0419) == 7 && oj::languageOfLangId(0x0411) == 0 && oj::languageOfLangId(9) == 0,
          "language by the language id");
}

// OpenJuice.ini's names, the letters' case aside.
OJ_UNIT_TEST(Language, ByName) {
    const auto polish = oj::Language::byName("Polish");
    check(polish && polish->index == oj::kPolish && oj::Language::byName("POLISH") &&
              oj::Language::byName("polish")->controlsLetter == 'p' && !oj::Language::byName("polski") &&
              !oj::Language::byName("pol"),
          "Polish, POLISH and polish alike, no other name");
}

// A language's files, English's where the data lacks them (a build without Polish, its texts alone: a player's).
OJ_UNIT_TEST(Language, EnglishInPlace) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "OpenJuice.Tests.Languages";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const std::filesystem::path scripts = root / "scripts", speech = root / "audio" / "speech";
    touch(scripts / "polish.txt");
    touch(scripts / "english.txt");
    touch(scripts / "controls_dx9_e.txt");
    touch(scripts / "jettexte.txt");
    touch(speech / "uie" / "uie.dsb");
    const oj::GameFiles files(root);
    const oj::Language polish = *oj::Language::byIndex(oj::kPolish);
    check(oj::textTableFile(files, polish) == std::filesystem::path("scripts") / "polish.txt",
          "the language's own text table where it is there");
    check(oj::controlsFile(files, polish) == std::filesystem::path("scripts") / "controls_dx9_e.txt" &&
              oj::jetTextFile(files, polish) == std::filesystem::path("scripts") / "jettexte.txt" &&
              oj::speechBank(files, "ui", 'p') == "uie",
          "no controls_dx9_p.txt, jettextp.txt, uip: English's");
    touch(scripts / "controls_dx9_p.txt");
    touch(speech / "uip" / "uip.dsb");
    check(oj::controlsFile(files, polish) == std::filesystem::path("scripts") / "controls_dx9_p.txt" &&
              oj::speechBank(files, "ui", 'p') == "uip",
          "the language's own where they are there");
    std::filesystem::remove(scripts / "controls_dx9_p.txt", ec);
    std::filesystem::remove(scripts / "controls_dx9_e.txt", ec);
    touch(scripts / "controls_dx9.txt");
    check(oj::controlsFile(files, polish) == std::filesystem::path("scripts") / "controls_dx9.txt",
          "neither: controls_dx9.txt");
    std::filesystem::remove(scripts / "controls_dx9.txt", ec);
    check(oj::controlsFile(files, polish) == std::filesystem::path("scripts") / "controls_dx9_p.txt" &&
              oj::speechBank(files, "race", 'p') == "racep",
          "none at all: the language's own (its reader says so)");
    std::filesystem::remove_all(root, ec);
}
