#pragma once
// The game's settings: HKEY_CURRENT_USER\Software\THQ\Juiced, written by JuicedConfig.exe.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

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
    uint32_t widescreen = 0;       // widescreen: the fixed build's picture shape (Render/Display)
    uint32_t multiTurn = 0;        // MultiTurn
    uint32_t threading = 1;        // threading

    // The fixed build's renderer (docs/fixes.md Added): the values JuicedConfigExt's settings program writes for its
    // plugin's renderer, its names and limits; missing ones as the program (OpenJuiceConfig) shows them, the screen
    // mode by Windowed (a value out of its range: the program's default, as it reads them).
    uint32_t displayMode = 0;      // DisplayMode: 0 full screen, 1 borderless, 2 windowed; missing: by Windowed
    uint32_t borderlessScale = 0;  // BorderlessScale: 0 the picture's shape kept (black bars), 1 stretched
    std::wstring monitor;          // Monitor: the GDI name (\\.\DISPLAY2); empty: the adapter's
    uint32_t refreshRate = 0;      // RefreshRate: full screen's, 0 the resolution's highest
    uint32_t msaa = 0;             // MSAA: the samples, 0 off
    uint32_t smaa = 0;             // SMAA: OpenJuice's own (Anti-aliasing SMAA in OpenJuiceConfig)
    uint32_t vsync = 1;            // VSync
    uint32_t maxFps = 0;           // MaxFPS: 1 .. 1000, 0 the refresh rate
    uint32_t textureFilter = 5;    // TextureFilter: 0 the game's, 1 trilinear, 2 .. 5 anisotropic 2x .. 16x
    uint32_t lodBias = 3;          // LodBias: 0 the game's (-1.5), 1 -1.0, 2 -0.5, 3 0, 4 +0.5
    uint32_t showFps = 0;          // ShowFps: the frame rate in the corner
    // The fixed build's effects, JuicedConfigExt's ini options as registry values (their ranges; out of one: the
    // default).
    // CarPaint: the car paint, glass and chrome lit per pixel.
    uint32_t carPaint = 0;             // CarPaint
    // WorldLight: the sky light of the world by the surface's direction (FixedEffects::worldLight's levels);
    // JuicedConfigExt's WorldLightStrength not read.
    uint32_t worldLight = 0;           // WorldLight: 0 the stock's, 1 HI, 2 ULTRA
    // Sky: the clouds (FixedEffects::sky's levels).
    uint32_t sky = 0;                  // Sky: 0 the stock's, 1 HI, 2 ULTRA
    // Headlights: the cars' headlights light the night road and walls at their full strength (the game's shaders
    // 3.0); JuicedConfigExt's HeadlightsStrength not read.
    uint32_t headlights = 0;           // Headlights: 0 the stock's, 1 the fixed build's, 2 those with their shadows
    // HeatHaze: the heat haze from the cars' exhausts.
    uint32_t heatHaze = 0;             // HeatHaze
    uint32_t heatHazeStrength = 50;    // HeatHazeStrength: 0 .. 50 % (past it, a former 300 at most: 50)
    // Reflections: the graphics options' REFLECTION ULTRA over HI (FixedEffects::reflections).
    uint32_t reflections = 0;          // Reflections
    // LightFlares: the graphics options' LIGHT FLARES CUSTOM over the stock's on (FixedEffects::lightFlares).
    uint32_t lightFlares = 0;          // LightFlares
    // AmbientOcclusion: the graphics options' AMBIENT OCCLUSION (FixedEffects::ambientOcclusion).
    uint32_t ambientOcclusion = 0;     // AmbientOcclusion: 0 OFF, 1 HI
    // Shadows: the graphics options' SHADOWS levels above the stock's own (FixedEffects::shadows).
    uint32_t shadows = 0;              // Shadows: 0 the stock's MED, 1 HI, 2 ULTRA

    // The defaults with the command line's audio switches applied (-x, -n).
    static Settings defaults(const LaunchOptions& options);
};

// The 16 bytes of "Adapter" (the DeviceIdentifier of the chosen adapter); nullopt without the key or the value.
std::optional<std::array<uint8_t, 16>> readAdapterId();

// The values present in the key replace those of settings.
void readSettings(Settings& settings);
// The key's `language` as readSettings reads it (-1 without the key or the value).
int32_t readLanguageSetting();

// The fixed build's value changed in the game (the graphics options' glow): written into the key (made when missing) as
// a DWORD; false when it cannot be.
bool writeSetting(const char* name, uint32_t value);
// A DWORD of the settings' key; none when it is not there.
std::optional<uint32_t> readSetting(const char* name);

}  // namespace oj
