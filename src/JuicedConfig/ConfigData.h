#pragma once
// What OpenJuiceConfig, the port of the stock game's JuicedConfig.exe (docs/spec/30-juiced-config.md), works out
// apart from its window: the language's texts, the display modes as the window's lists keep them, the sound's flags
// and the shaders' level.

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace oj::config {

// The language whose texts the window shows ([0x00412040]), numbered as the game's. The reference build was made for
// another region: its tool fixes the language to that region's (7, the default, set again as the window is filled,
// 0x004025EA). OpenJuiceConfig has the region binding removed and picks the language as OpenJuice's game does.
enum class Language : int32_t {
    kEnglish = 0,
    kFrench = 1,
    kGerman = 2,
    kItalian = 3,
    kSpanish = 4,
    kCzech = 5,
    kPolish = 6,
    kRussian = 7,
};

// The language of a Windows language id as the game takes it (Core/Data/Language.h, languageOfLangId: the English
// build 1.0.1's choice with Czech, Polish and Russian added): by its primary language German (0x07), Spanish (0x0A),
// French (0x0C), Italian (0x10), Czech (0x05), Polish (0x15), Russian (0x19); any other English.
Language languageOfLangId(uint32_t langId);
// A language by its name in OpenJuice.ini's [Game] Language (english, french, german, italian, spanish, czech, polish,
// russian, as the game's); none for another.
std::optional<Language> languageByName(std::string_view name);
// The tool's language as the game picks its own (Platform's chooseLanguage): the settings' `language` when it is not
// negative, else the user's language id; [Game] Language's name, when it is one, instead.
Language chooseLanguage(int32_t settingsLanguage, uint32_t userLangId, std::string_view configLanguage);

// A text's string id: each text has eight ids in a row, in the order English, French, Italian, German, Spanish, Czech,
// Polish, Russian; German takes the fourth and Italian the third (every LoadStringA of the tool).
uint32_t textId(uint32_t first, Language language);

// A display mode as the mode list keeps it (an item's data, [0x00412044]): the width shifted 12 bits up, the height,
// and bit 31 for 16 bits per pixel (else 32). Stock problem: widths from 4096 lose their high bits (5120 reads as
// 1024).
uint32_t modeKey(uint32_t width, uint32_t height, bool sixteenBits);
inline uint32_t modeWidth(uint32_t key) { return (key >> 12) & 0xFFF; }
inline uint32_t modeHeight(uint32_t key) { return key & 0xFFF; }
inline uint32_t modeDepth(uint32_t key) { return (key & 0x80000000u) != 0 ? 16u : 32u; }
// 0x00402030: the order of the full screen modes: by width, height, then depth.
bool modeLess(uint32_t a, uint32_t b);
// The mode list's item nearest to the mode kept (the sum of the squared differences of depth, width and height, as
// unsigned values: a wrapped sum is far); the first of equals, 0 for an empty list.
size_t nearestMode(const std::vector<uint32_t>& keys, uint32_t kept);
// 800 x 600 at 32 bits, the mode kept before the registry's is read ([0x00412044]).
inline constexpr uint32_t kDefaultMode = (800u << 12) | 600u;

// The modes the windowed list offers ([0x004120D8]), those within the desktop's display mode.
struct WindowedMode {
    uint32_t width;
    uint32_t height;
};
inline constexpr std::array<WindowedMode, 5> kWindowedModes = {{
    {640, 480}, {800, 600}, {1024, 768}, {1280, 960}, {1600, 1200}}};

// The sound's check boxes ([0x00412048] .. [0x00412058]) and the registry's audioflags they make.
struct AudioChecks {
    bool speech = true;          // [0x00412048]
    bool music = true;           // [0x0041204C]
    bool hardwareMixing = true;  // [0x00412050]
    bool eax = true;             // [0x00412054]
    bool allAudio = true;        // [0x00412058]
};
// 0x004017D0: bit 0 no sound, bit 1 hardware mixing (else bit 2), bit 3 no music, bit 4 no speech, bit 5 no EAX.
uint32_t audioFlags(const AudioChecks& checks);
// 0x00402A1E .. 0x00402A70: the check boxes of a registry's audioflags (EAX as the flags say; the tool then clears it
// without EAX). Hardware mixing is bit 1 alone.
AudioChecks audioChecks(uint32_t flags);
inline constexpr uint32_t kDefaultAudioFlags = 2;  // [0x0041205C]: hardware mixing

// 0x00401000 over the adapter's caps: 1 below pixel shader 1.0, 2 below 2.0, 3 below 3.0 or below vertex shader 3.0,
// else 4. The shaders' list offers one item fewer than the level (none, 1.x, 2.0, all).
int shaderLevel(uint32_t vertexShaderVersion, uint32_t pixelShaderVersion);

}  // namespace oj::config
