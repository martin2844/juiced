#pragma once
// The fixed build's renderer options (docs/fixes.md Added), JuicedConfigExt's renderer as the registry gives it
// (Game/Platform/Settings: TextureFilter, LodBias, ShowFps, VSync, MaxFPS, BorderlessScale), what they decide apart
// from the device: the sampler states the game asks for as the texture filter and the LOD bias change them, the frame
// rate counter's rectangles, the frame limiter's interval, the picture's place in the window without a frame.

#include "Render/RenderStates.h"

#include <cstdint>
#include <vector>

namespace oj::render {

// The textures of the game ask for a mip LOD bias of -1.5, the world's mostly for linear filtering, the UI's pictures
// for point filtering (their .ddt headers, docs/spec/31-file-layouts.md): far textures shimmer. TextureFilter 0 the
// game's, 1 trilinear, 2 .. 5 anisotropic 2x .. 16x (at most the adapter's); LodBias 0 the game's, 1 .. 4: -1.0,
// -0.5, 0, +0.5.
struct TextureQuality {
    bool filter = false;     // a filter of its own
    uint32_t anisotropy = 0;  // 0: none (trilinear)
    bool bias = false;       // a LOD bias of its own
    float lodBias = 0.0f;

    // anisotropicCaps: the adapter's TextureFilterCaps has D3DPTFILTERCAPS_MINFANISOTROPIC; maxAnisotropy its
    // MaxAnisotropy.
    static TextureQuality make(uint32_t textureFilter, uint32_t lodBias, bool anisotropicCaps, uint32_t maxAnisotropy);
    bool any() const { return filter || bias; }

    // A sampler state the game sets, as the options change it (the plugin's TextureQuality::Adjust): MINFILTER point
    // or linear anisotropic (else linear), MAGFILTER point linear, MIPFILTER point or none linear, MAXANISOTROPY the
    // anisotropy, MIPMAPLODBIAS the bias (its float's bits); any other state or value as it is.
    uint32_t adjust(SamplerState type, uint32_t value) const;
};

// A picture's level (0xAARRGGBB texels, width x height) for the texture filter: each texel of alpha 0 takes the
// mean colour of its seen neighbours (alpha above 0 or coloured so before), pass by pass outwards (16 at most), its
// alpha kept. The game's pictures leave their transparent texels white (the HUD's dials): sampled linearly, the white
// came into their edges; point sampled, or blended at alpha 0, nothing seen changes.
void bleedTransparent(uint32_t* texels, uint32_t width, uint32_t height);

// A rectangle of the back buffer in pixels and its colour (0xAARRGGBB).
struct FpsRect {
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    uint32_t argb = 0;
};

// The frame rate counter of the plugin (FpsCounter) at the back buffer's top left: a dark background and seven
// segment digits in amber, its unit the height / 360 (at least 1): the background first, then every segment.
std::vector<FpsRect> fpsCounterRects(uint32_t value, uint32_t backBufferHeight);

// Frames counted and the rate shown, again every half second or more (at most 9999; 0 until the first).
class FrameRateCounter {
public:
    void frame(double seconds);
    uint32_t shown() const { return m_shown; }

private:
    double m_start = -1.0;
    uint32_t m_frames = 0;
    uint32_t m_shown = 0;
};

// The frames per second the limiter holds (the plugin's FrameRate::Resolve): without VSync MaxFPS when it gives one,
// else the display's refresh rate above 60; 0: the stock limiter's 60.1 (a refresh rate of 60 or less).
uint32_t frameRateLimit(bool vsync, uint32_t maxFps, uint32_t refreshRate);
// The limiter's interval in seconds: 1 / (limit + 0.1) as the stock's 1 / 60.1, so that with VSync the display sets
// the pace; 0 with -r (no limit).
double frameInterval(uint32_t limit, bool presentIntervalOne);
// The limiter's wait after the present (the stock's): it yields (Sleep(0)) until `interval` seconds have passed since
// `last` (the performance counter's ticks the previous wait ended at); the ticks it ends at. Its own clock: it holds
// the limit whatever the present does (DXVK's returns at once).
int64_t waitFrame(int64_t last, double interval);

// The place of a width x height picture in a window of the monitor's size (BorderlessScale): the largest rectangle of
// its shape in the middle, black bars around; stretched: the whole window.
struct PictureRect {
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
PictureRect placePicture(uint32_t windowWidth, uint32_t windowHeight, uint32_t width, uint32_t height, bool stretch);

}  // namespace oj::render
