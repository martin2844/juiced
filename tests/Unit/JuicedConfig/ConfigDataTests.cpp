// Unit tests: what OpenJuiceConfig works out apart from its window (JuicedConfig/ConfigData.h): its language and the
// texts' ids by language, the display modes as the mode list keeps them, the sound's flags and the shaders' level.

#include "Framework/Test.h"

#include "Core/Data/Language.h"
#include "JuicedConfig/ConfigData.h"

#include <optional>
#include <vector>

namespace {

using oj::test::check;
namespace config = oj::config;

}  // namespace

// Eight ids a text: Italian takes the third and German the fourth; Russian the eighth.
OJ_UNIT_TEST(JuicedConfigData, TextIds) {
    using config::Language;
    check(config::textId(110, Language::kEnglish) == 110 && config::textId(110, Language::kFrench) == 111,
          "English and French: the first two");
    check(config::textId(110, Language::kItalian) == 112 && config::textId(110, Language::kGerman) == 113,
          "Italian the third, German the fourth");
    check(config::textId(110, Language::kSpanish) == 114 && config::textId(110, Language::kPolish) == 116,
          "Spanish and Polish by their own numbers");
    check(config::textId(318, Language::kRussian) == 325, "Russian: the last of eight");
}

// The tool takes a Windows language id's language as the game does (Core's languageOfLangId), sub-languages alike.
OJ_UNIT_TEST(JuicedConfigData, LanguageAsGame) {
    bool same = true;
    for (uint32_t id = 0; id < 0x10000 && same; ++id) {
        same = static_cast<int>(config::languageOfLangId(id)) == oj::languageOfLangId(id);
    }
    check(same, "every language id: the game's language");
    check(config::languageOfLangId(0x0419) == config::Language::kRussian &&
              config::languageOfLangId(0x0807) == config::Language::kGerman &&
              config::languageOfLangId(0x0409) == config::Language::kEnglish &&
              config::languageOfLangId(0x0411) == config::Language::kEnglish,
          "Russian, Swiss German, English, Japanese (English)");
}

// The tool's languages are the game's, in the game's order: each of the game's eight by its index and its
// OpenJuice.ini name.
OJ_UNIT_TEST(JuicedConfigData, LanguagesAsGame) {
    bool same = true;
    for (int i = 0; i < 8; ++i) {
        const std::optional<oj::Language> game = oj::Language::byIndex(i);
        same = same && game && config::languageByName(game->name) == static_cast<config::Language>(i);
    }
    check(same, "the eight languages: the game's names at the game's indexes");
    check(!oj::Language::byIndex(8) && !config::languageByName("japanese"), "no ninth in either");
}

// The settings' language before the user's, OpenJuice.ini's name before both (an unknown name passed over).
OJ_UNIT_TEST(JuicedConfigData, LanguageChoice) {
    using config::Language;
    check(config::chooseLanguage(-1, 0x0407, "") == Language::kGerman, "no settings' language: the user's");
    check(config::chooseLanguage(0x0419, 0x0409, "") == Language::kRussian, "the settings' language first");
    check(config::chooseLanguage(0x0419, 0x0409, "french") == Language::kFrench, "the ini's name before both");
    check(config::chooseLanguage(-1, 0x0415, "klingon") == Language::kPolish, "an unknown name passed over");
    check(!config::languageByName("German") && config::languageByName("czech") == Language::kCzech,
          "the names as the game's, in lower case");
}

// A mode's key: the width 12 bits up, the height, bit 31 for 16 bits per pixel; widths from 4096 lose their high
// bits as the stock's do.
OJ_UNIT_TEST(JuicedConfigData, ModeKeys) {
    const uint32_t key = config::modeKey(1024, 768, true);
    check(key == (0x80000000u | (1024u << 12) | 768u), "1024 x 768 x 16's key");
    check(config::modeWidth(key) == 1024 && config::modeHeight(key) == 768 && config::modeDepth(key) == 16,
          "its width, height and depth back");
    check(config::modeDepth(config::modeKey(800, 600, false)) == 32, "32 bits without the bit");
    check(config::kDefaultMode == config::modeKey(800, 600, false), "the mode kept at first: 800 x 600 x 32");
    check(config::modeWidth(config::modeKey(5120, 2880, false)) == 1024, "5120 wide reads as 1024");
}

// The full screen list's order: width, height, then depth.
OJ_UNIT_TEST(JuicedConfigData, ModeOrder) {
    check(config::modeLess(config::modeKey(800, 600, true), config::modeKey(800, 600, false)),
          "16 bits before 32 at the same size");
    check(config::modeLess(config::modeKey(640, 480, false), config::modeKey(800, 600, true)),
          "the narrower first whatever its depth");
    check(config::modeLess(config::modeKey(1280, 960, false), config::modeKey(1280, 1024, false)),
          "the lower first at the same width");
    check(!config::modeLess(config::modeKey(800, 600, false), config::modeKey(800, 600, false)), "none before itself");
}

// The item kept selected: the nearest by the squared differences of depth, width and height, the first of equals.
OJ_UNIT_TEST(JuicedConfigData, NearestMode) {
    const std::vector<uint32_t> modes = {config::modeKey(640, 480, false), config::modeKey(800, 600, true),
                                         config::modeKey(800, 600, false), config::modeKey(1024, 768, false)};
    check(config::nearestMode(modes, config::modeKey(800, 600, false)) == 2, "the mode itself");
    check(config::nearestMode(modes, config::modeKey(1000, 700, false)) == 3, "1000 x 700: 1024 x 768");
    check(config::nearestMode(modes, config::modeKey(1024, 768, true)) == 3,
          "a depth missing: the same size at the other depth");
    check(config::nearestMode({config::modeKey(800, 600, false), config::modeKey(800, 600, false)},
                              config::kDefaultMode) == 0,
          "the first of equals");
    check(config::nearestMode({}, config::kDefaultMode) == 0, "an empty list: item 0");
}

// The registry's audioflags: bit 0 no sound, bit 1 hardware mixing (else bit 2), bit 3 no music, bit 4 no speech,
// bit 5 no EAX; read back, hardware mixing is bit 1 alone.
OJ_UNIT_TEST(JuicedConfigData, AudioFlags) {
    check(config::audioFlags(config::AudioChecks{}) == config::kDefaultAudioFlags, "every box checked: 2");
    const config::AudioChecks none{false, false, false, false, false};
    check(config::audioFlags(none) == 0x3D, "none checked: 0x3D");
    const config::AudioChecks read = config::audioChecks(0x3D);
    check(!read.speech && !read.music && !read.hardwareMixing && !read.eax && !read.allAudio, "0x3D: none back");
    const config::AudioChecks all = config::audioChecks(config::kDefaultAudioFlags);
    check(all.speech && all.music && all.hardwareMixing && all.eax && all.allAudio, "2: every box back");
    check(!config::audioChecks(4).hardwareMixing && config::audioChecks(6).hardwareMixing,
          "hardware mixing by bit 1 alone");
}

// The level of the adapter's shaders: one more than the shaders' items the list offers.
OJ_UNIT_TEST(JuicedConfigData, ShaderLevel) {
    check(config::shaderLevel(0xFFFE0101u, 0) == 1, "no pixel shaders: 1");
    check(config::shaderLevel(0xFFFE0101u, 0xFFFF0104u) == 2, "pixel shaders 1.4: 2");
    check(config::shaderLevel(0xFFFE0200u, 0xFFFF0200u) == 3, "2.0: 3");
    check(config::shaderLevel(0xFFFE0200u, 0xFFFF0300u) == 3, "pixel shaders 3.0 with vertex shaders 2.0: 3");
    check(config::shaderLevel(0xFFFE0300u, 0xFFFF0300u) == 4, "3.0: 4");
}
