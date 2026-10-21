#pragma once
// The renderer's settings (the object 0x0079E600; docs/spec/12-rendering.md "The graphics settings"): the screen,
// the effects the graphics options window switches, the detail and the adapter's numbers. Its defaults come by the
// adapter's class (0x005A26C0), the renderer's start sets the screen and the detail (0x005A25F0), the adapter its
// numbers (0x00512A40), and reg.cf1 keeps the window's values with the driver.

#include "Core/Saves/Save.h"

#include <array>
#include <cstdint>

namespace oj {

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
};

// The object of the game; made with the defaults of class 3 (0x006C37B0: 0x005A26C0 before any adapter is known).
GraphicsConfig& graphicsConfig();

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

}  // namespace oj
