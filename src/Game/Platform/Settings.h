#pragma once
// The game's settings: HKEY_CURRENT_USER\Software\THQ\Juiced, written by JuicedConfig.exe.

#include <array>
#include <cstdint>
#include <optional>

namespace oj {

struct LaunchOptions;

struct Settings {
    uint32_t width = 800;          // RezWidth
    uint32_t height = 600;         // RezHeight
    uint32_t depth = 16;           // RezDepth: 16 | 32
    uint32_t antialiasing = 1;     // Antialiasing
    uint32_t shaders = 99;         // Shaders
    uint32_t windowed = 1;         // Windowed
    int32_t language = -1;         // language (-1 also when the value is missing)
    uint32_t audioFlags = 2;       // audioflags
    uint32_t widescreen = 0;       // widescreen
    uint32_t multiTurn = 0;        // MultiTurn
    uint32_t threading = 1;        // threading

    // The defaults with the command line's audio switches applied (-x, -n).
    static Settings defaults(const LaunchOptions& options);
};

// The 16 bytes of "Adapter" (the DeviceIdentifier of the chosen adapter); nullopt without the key or the value.
std::optional<std::array<uint8_t, 16>> readAdapterId();

// The values present in the key replace those of settings.
void readSettings(Settings& settings);
// The key's `language` as readSettings reads it (-1 without the key or the value).
int32_t readLanguageSetting();

}  // namespace oj
