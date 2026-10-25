#include "JuicedConfig/ConfigData.h"

#include <iterator>

namespace oj::config {

uint32_t textId(uint32_t first, Language language) {
    switch (language) {
    case Language::kGerman: return first + 3;
    case Language::kItalian: return first + 2;
    default: return first + static_cast<uint32_t>(language);
    }
}

Language languageOfLangId(uint32_t langId) {
    switch (langId & 0x3FF) {
    case 0x07: return Language::kGerman;
    case 0x0A: return Language::kSpanish;
    case 0x0C: return Language::kFrench;
    case 0x10: return Language::kItalian;
    case 0x05: return Language::kCzech;
    case 0x15: return Language::kPolish;
    case 0x19: return Language::kRussian;
    default: return Language::kEnglish;
    }
}

std::optional<Language> languageByName(std::string_view name) {
    constexpr std::string_view kNames[] = {"english", "french", "german", "italian",
                                           "spanish", "czech",  "polish", "russian"};
    for (size_t i = 0; i < std::size(kNames); ++i) {
        if (name == kNames[i]) return static_cast<Language>(i);
    }
    return std::nullopt;
}

Language chooseLanguage(int32_t settingsLanguage, uint32_t userLangId, std::string_view configLanguage) {
    Language language =
        languageOfLangId(settingsLanguage >= 0 ? static_cast<uint32_t>(settingsLanguage) : userLangId);
    if (const std::optional<Language> named = languageByName(configLanguage)) language = *named;
    return language;
}

uint32_t modeKey(uint32_t width, uint32_t height, bool sixteenBits) {
    return (sixteenBits ? 0x80000000u : 0u) | ((width & 0x7FFF) << 16) | (height & 0xFFFF);
}

bool modeLess(uint32_t a, uint32_t b) {
    if (modeWidth(a) != modeWidth(b)) return modeWidth(a) < modeWidth(b);
    if (modeHeight(a) != modeHeight(b)) return modeHeight(a) < modeHeight(b);
    return static_cast<int32_t>(modeDepth(a)) < static_cast<int32_t>(modeDepth(b));
}

size_t nearestMode(const std::vector<uint32_t>& keys, uint32_t kept) {
    size_t best = 0;
    uint32_t bestDistance = 0xFFFFFFFFu;
    for (size_t i = 0; i < keys.size(); ++i) {
        const uint32_t key = keys[i];
        const uint32_t depth = modeDepth(kept) - modeDepth(key);
        const uint32_t width = modeWidth(kept) - modeWidth(key);
        const uint32_t height = modeHeight(kept) - modeHeight(key);
        const uint32_t distance = height * height + width * width + depth * depth;
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

uint32_t audioFlags(const AudioChecks& checks) {
    uint32_t flags = checks.allAudio ? 0u : 1u;
    flags |= checks.hardwareMixing ? 2u : 4u;
    if (!checks.speech) flags |= 0x10;
    if (!checks.music) flags |= 8;
    if (!checks.eax) flags |= 0x20;
    return flags;
}

AudioChecks audioChecks(uint32_t flags) {
    AudioChecks c;
    c.allAudio = (flags & 1) == 0;
    c.music = (flags & 8) == 0;
    c.speech = (flags & 0x10) == 0;
    c.hardwareMixing = (flags & 2) != 0;
    c.eax = (flags & 0x20) == 0;
    return c;
}

int shaderLevel(uint32_t vertexShaderVersion, uint32_t pixelShaderVersion) {
    if (pixelShaderVersion < 0xFFFF0100u) return 1;
    if (pixelShaderVersion < 0xFFFF0200u) return 2;
    if (pixelShaderVersion < 0xFFFF0300u) return 3;
    if (vertexShaderVersion < 0xFFFE0300u) return 3;
    return 4;
}

}  // namespace oj::config
