#pragma once
// The renderer's settings (the object 0x0079E600; docs/spec/12-rendering.md "The graphics settings"): the screen,
// the effects the graphics options window switches, the detail and the adapter's numbers. Its defaults come by the
// adapter's class (0x005A26C0), the renderer's start sets the screen and the detail (0x005A25F0), the adapter its
// numbers (0x00512A40), and reg.cf1 keeps the window's values with the driver.

#include "Core/Saves/Save.h"

#include <array>
#include <cstdint>

namespace oj {

// The fixed build's effects: the registry's values of JuicedConfigExt's settings program (Game/Platform/Settings), read
// at the start, changed in the graphics options' ADVANCED page and kept in the registry again (docs/fixes.md Added).
struct FixedEffects {
    uint32_t carPaint = 0;              // CarPaint
    // WorldLight: MED the stock's (0), HI the sky light by the surface's direction (1), ULTRA (2) the same for now.
    uint32_t worldLight = 0;
    // Sky: MED the stock's clouds (0), HI the procedural clouds' layer (1), ULTRA the procedural clouds as a volume (2)
    // (Game/Scene/Clouds), their shadows on the ground.
    uint32_t sky = 0;
    uint32_t headlights = 0;            // Headlights: the stock's (0), the fixed build's (1), with their shadows (2)
    uint32_t heatHaze = 0;              // HeatHaze
    uint32_t heatHazeStrength = 50;     // HeatHazeStrength: 0 .. 50 % (the ADVANCED page's ten steps)
    // The picture's anti-aliasing and texture filter (Render/RenderDevice's setAntialiasing and setTextureFilter).
    uint32_t msaa = 0;                  // MSAA: the samples, 0 off
    uint32_t smaa = 0;                  // SMAA
    uint32_t textureFilter = 5;         // TextureFilter: 0 the game's, 1 trilinear, 2 .. 5 anisotropic 2x .. 16x
    // Reflections: 1 ULTRA over the stock's reflections HI (the graphics options' REFLECTION row): the car the camera
    // looks at reflects the other cars near it (Game/Scene/TrackScene's cars' cube).
    uint32_t reflections = 0;
    // LightFlares: 1 CUSTOM over the stock's LIGHT FLARES on (the graphics options' GLOW row): the glow with the
    // exposure at most kCustomExposure (Game/Scene/PostEffects; the stock's goes to 2 and overexposes the night).
    uint32_t lightFlares = 0;
    // AmbientOcclusion: 1 the fixed build's SSAO over the view (the graphics options' AMBIENT OCCLUSION row's HI;
    // Game/Scene/AmbientOcclusion's depth serves the other passes either way).
    uint32_t ambientOcclusion = 0;
    // Shadows: the levels of the SHADOWS row above the stock's own, 1 the cars' shadows from the street lamps (HI), 2
    // with the sun's cascades (ULTRA) (Game/Scene/LampShadows, SunShadows; the stock's SHADOWS off takes them with it).
    uint32_t shadows = 0;

    bool operator==(const FixedEffects&) const = default;
};

struct GraphicsConfig {
    uint32_t width = 0;                      // +0x00
    uint32_t height = 0;                     // +0x04
    uint32_t depth = 0;                      // +0x08: the colour bits
    uint32_t windowed = 0;                   // +0x0C: the registry's Windowed ([0x0071E5C4])
    uint32_t cubeSize = 0;                   // +0x10: the renderer's cubes and the cars' reflection cube
    uint32_t worldCubeSize = 0;              // +0x14: the world's reflection cube
    uint32_t shadowSize = 0;                 // +0x18: the cars' silhouette shadow target (Game/Scene/Silhouette)
    uint32_t wideBlurWidth = 0;              // +0x1C: the post effects' blur target +0x1C
    uint32_t wideBlurHeight = 0;             // +0x20
    uint32_t blurWidth = 0;                  // +0x24: the post effects' blur target +0x14
    uint32_t blurHeight = 0;                 // +0x28
    uint32_t reflectionWidth = 0;            // +0x2C: the wet road's reflection
    uint32_t reflectionHeight = 0;           // +0x30
    uint32_t sceneWidth = 0;                 // +0x34: the post effects' scene targets (0 without shaders)
    uint32_t sceneHeight = 0;                // +0x38
    std::array<uint32_t, 2> unusedFlags{};   // +0x3C, +0x40: 1 by the defaults, read by nothing found
    uint32_t antialiasing = 0;               // +0x44: the registry's Antialiasing ([0x0071E5C8])
    int32_t shadows = 0;                     // +0x48: 1 off, 2 on
    int32_t reflections = 0;                 // +0x4C: 0 .. 3
    float detail = 0.0f;                     // +0x50: 0 .. 1
    int32_t motionBlur = 0;                  // +0x54
    int32_t glow = 0;                        // +0x58
    int32_t depthOfField = 0;                // +0x5C
    float brightness = 0.0f;                 // +0x60: 0 .. 1
    int32_t noShaders = 0;                   // +0x64: the adapter has no shaders
    int32_t gamma = 0;                       // +0x68: full screen with D3DCAPS2_FULLSCREENGAMMA
    int32_t shaders = 0;                     // +0x6C: the adapter has shaders
    float lodBias = 0.0f;                    // +0x70 ([0x0079E670]): the detail x 1.5 + 0.66
    uint32_t vendor = 0;                     // +0x74: the adapter's vendor id
    uint32_t device = 0;                     // +0x78: the adapter's device id
    int32_t fov = 0;                         // the fixed build's race view, reg.cf1's eighth value (fovScale)
    FixedEffects effects;  // the fixed build's
};

// The sky light's shaping of WORLD LIGHT HI and ULTRA (JuicedConfigExt's WorldLightStrength / 100: sky 0.2 and side
// 0.15 of the shaders' literals times this; the registry's WorldLightStrength not read).
inline constexpr float kWorldLightShape = 1.5f;
// The highest exposure of LIGHT FLARES CUSTOM: between OFF (1, no glow) and STOCK (2), a dark picture brightened half as
// far as the stock's (the user, 2026-10-28: at 1 CUSTOM looked as OFF); a bright one dimmed as the stock's.
inline constexpr float kCustomExposure = 1.5f;

// The object of the game; made with the defaults of class 3 (0x006C37B0: 0x005A26C0 before any adapter is known).
GraphicsConfig& graphicsConfig();

// The fixed build's REFLECTIONS ULTRA: the stock's HI (3) with the shaders and the effects' Reflections.
inline bool ultraReflections(const GraphicsConfig& c) {
    return c.noShaders == 0 && c.reflections == 3 && c.effects.reflections != 0;
}

// The fixed build's WORLD LIGHT HI and ULTRA: the street lamps light the cars (TrackScene::setCarLamps, the paint's
// CarPaint3 through setHeadlights' CarHeadlight; the user, 2026-10-29: this is light, so it belongs to the light's row
// and not to REFLECTION ULTRA, which it used to hang off).
inline bool carLampsLit(const GraphicsConfig& c) {
    return c.noShaders == 0 && c.effects.worldLight >= 1;
}

// The fixed build's SHADOWS row (the user, 2026-10-29: its own levels, moved to the ADVANCED page): 0 OFF (the stock's
// off: its silhouettes and none of ours), 1 MED (the stock's silhouettes), 2 HI (with the cars' shadows from the street
// lamps), 3 ULTRA (with the sun's cascades in place of the baked sun shadow). The stock's own field keeps its 1 and 2
// (Silhouette, Projector); ours are the levels above it (FixedEffects::shadows, as REFLECTION's ULTRA does).
inline int shadowLevel(const GraphicsConfig& c) {
    if (c.noShaders != 0 || c.shadows != 2) return 0;
    return 1 + static_cast<int>(c.effects.shadows > 2 ? 2 : c.effects.shadows);
}
inline void setShadowLevel(GraphicsConfig& c, int level) {
    c.shadows = level > 0 ? 2 : 1;
    c.effects.shadows = static_cast<uint32_t>(level > 2 ? 2 : (level > 1 ? 1 : 0));
}

// 0x005A26C0's class of an adapter: 0 .. 4 (3 for one not listed).
int graphicsClass(uint32_t vendor, uint32_t device);

// 0x005A26C0: the defaults of the adapter's class (the table 0x00725798: shadows, reflections, detail, motion blur,
// glow, depth of field) with the screen of 0x005A2490.
void graphicsDefaults(GraphicsConfig& config);

// The registry's values 0x005A25F0 starts from ([0x0071E5B8] .. [0x0071E5C8], read by 0x0059C5E0; their defaults
// 800, 600, 16, 1, 1).
struct GraphicsStart {
    uint32_t width = 800;        // RezWidth
    uint32_t height = 600;       // RezHeight
    uint32_t depth = 16;         // RezDepth
    uint32_t windowed = 1;       // Windowed
    uint32_t antialiasing = 1;   // Antialiasing
};

// 0x005A25F0: the renderer's start; the screen of the registry (its size also +0x34, +0x38), windowed and
// antialiasing, the detail 60 % ([0x0071E5CC]) and the factor from it, the reflections 2 (0 without shaders), the
// effects off without shaders.
void graphicsStart(GraphicsConfig& config, const GraphicsStart& start = {});

// 0x00512A40: the adapter's numbers and whether it has shaders; without shaders the shadows off and the effects 0.
// +0x68: the adapter's D3DCAPS2_FULLSCREENGAMMA out of a window (the brightness's gamma ramp, 0x00512920).
void graphicsAdapter(GraphicsConfig& config, uint32_t vendor, uint32_t device, bool shaders,
                     bool fullScreenGamma = false);

// 0x005A2580, at the renderer's creation (0x0051347F, after 0x00512A40): the motion blur, the glow and the depth of
// field 1 when the adapter has shaders (+0x64 0 and +0x6C set), else 0; before reg.cf1 is read.
void graphicsPostEffects(GraphicsConfig& config);

// 0x005A3410: the values of reg.cf1 (shadows, reflections, detail, motion blur, glow, depth of field, brightness)
// taken when the file holds; without shaders the effects off again.
void graphicsFromRegCf1(GraphicsConfig& config, const save::RegCf1& file);

// 0x005A33A0: reg.cf1 of the values.
save::RegCf1 graphicsRegCf1(const GraphicsConfig& config);

// The detail factor of a detail (the graphics options window: x87 at 53 bits).
float detailFactor(float detail);

// The fixed build's field of view of the race's cameras (its own; the game options' FOV): kept in reg.cf1's eighth
// value (0 in a stock file, which the stock never reads) as the permille of the stock's view, the tangents of its half
// angles; 0 the stock's (1); within kFovLeast .. kFovMost.
inline constexpr float kFovLeast = 0.6f;
inline constexpr float kFovMost = 1.8f;
float fovScale(int32_t fov);
// The kept field of view of a scale (the permille, held within the least and the most).
int32_t fovSetting(float scale);

}  // namespace oj
