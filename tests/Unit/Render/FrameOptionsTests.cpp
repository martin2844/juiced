// Unit tests: the fixed build's renderer options (Render/FrameOptions.h): the texture filter and LOD bias over the
// game's sampler states, the frame rate counter, the frame limiter, the picture's place in the window without a frame.

#include "Framework/Test.h"

#include "Render/FrameOptions.h"

#include <bit>
#include <cmath>

namespace {

using oj::test::check;
using oj::render::SamplerState;
using oj::render::TextureFilter;
using oj::render::TextureQuality;

}  // namespace

// TextureFilter 0 and LodBias 0: the game's states as they are.
OJ_UNIT_TEST(FrameOptions, GameTextures) {
    const TextureQuality q = TextureQuality::make(0, 0, true, 16);
    check(!q.any(), "nothing of its own");
    check(q.adjust(SamplerState::MinFilter, TextureFilter::Point) == TextureFilter::Point &&
              q.adjust(SamplerState::MagFilter, TextureFilter::Point) == TextureFilter::Point &&
              q.adjust(SamplerState::MipFilter, TextureFilter::None) == TextureFilter::None &&
              q.adjust(SamplerState::MipMapLodBias, std::bit_cast<uint32_t>(-1.5f)) == std::bit_cast<uint32_t>(-1.5f),
          "point, none and -1.5 as the game sets them");
}

// 1 trilinear; 2 .. 5 anisotropic 2x .. 16x at most the adapter's, linear without its caps; a state other than the
// filters' as it is.
OJ_UNIT_TEST(FrameOptions, TextureFilter) {
    const TextureQuality tri = TextureQuality::make(1, 0, true, 16);
    check(tri.any() && tri.anisotropy == 0, "trilinear: no anisotropy");
    check(tri.adjust(SamplerState::MinFilter, TextureFilter::Point) == TextureFilter::Linear &&
              tri.adjust(SamplerState::MagFilter, TextureFilter::Point) == TextureFilter::Linear &&
              tri.adjust(SamplerState::MipFilter, TextureFilter::None) == TextureFilter::Linear &&
              tri.adjust(SamplerState::MipFilter, TextureFilter::Point) == TextureFilter::Linear &&
              tri.adjust(SamplerState::MaxAnisotropy, 1) == 1,
          "trilinear: min, mag and mip linear, the anisotropy the game's");
    const TextureQuality an = TextureQuality::make(5, 0, true, 8);
    check(an.anisotropy == 8, "16x on an adapter of 8x: 8");
    check(an.adjust(SamplerState::MinFilter, TextureFilter::Linear) == TextureFilter::Anisotropic &&
              an.adjust(SamplerState::MagFilter, TextureFilter::Linear) == TextureFilter::Linear &&
              an.adjust(SamplerState::MaxAnisotropy, 1) == 8 &&
              an.adjust(SamplerState::AddressU, 3) == 3,
          "anisotropic: min anisotropic at 8, mag as asked, the address as it is");
    const TextureQuality noCaps = TextureQuality::make(3, 0, false, 16);
    check(noCaps.anisotropy == 0 && noCaps.adjust(SamplerState::MinFilter, TextureFilter::Point) == TextureFilter::Linear,
          "4x without the adapter's caps: linear");
}

// LodBias 1 .. 4: -1.0, -0.5, 0, +0.5 in place of the game's.
OJ_UNIT_TEST(FrameOptions, LodBias) {
    const float bias[] = {-1.0f, -0.5f, 0.0f, 0.5f};
    for (uint32_t i = 1; i <= 4; ++i) {
        const TextureQuality q = TextureQuality::make(0, i, true, 16);
        check(q.any() && q.adjust(SamplerState::MipMapLodBias, std::bit_cast<uint32_t>(-1.5f)) ==
                             std::bit_cast<uint32_t>(bias[i - 1]),
              "the LOD bias of the setting");
        check(q.adjust(SamplerState::MinFilter, TextureFilter::Point) == TextureFilter::Point, "the filter the game's");
    }
}

// The counter at the top left: the background, then the segments of each digit (60: six and six); its unit the
// height / 360 (720: 2; 240: 1); past 9999 four nines.
OJ_UNIT_TEST(FrameOptions, CounterRects) {
    const auto sixty = oj::render::fpsCounterRects(60, 720);
    check(sixty.size() == 13, "60: the background and 6 + 6 segments");
    check(sixty[0].x0 == 4 && sixty[0].y0 == 4 && sixty[0].x1 == 32 && sixty[0].y1 == 26 && sixty[0].argb == 0xFF101010,
          "720: the background (4, 4) .. (32, 26), dark");
    check(sixty[1].x0 == 8 && sixty[1].y0 == 8 && sixty[1].x1 == 16 && sixty[1].y1 == 10 && sixty[1].argb == 0xFFFFC800,
          "720: the 6's top segment (8, 8) .. (16, 10), amber");
    const auto small = oj::render::fpsCounterRects(1, 240);
    check(small.size() == 3 && small[1].x1 - small[1].x0 == 1 && small[1].y1 - small[1].y0 == 4,
          "240: 1 as two segments one pixel wide, 4 high");
    check(oj::render::fpsCounterRects(12345, 1080).size() == 1 + 4 * 6, "12345: 9999");
}

// The rate shown again every half second: 0 until the first, 60 at frames 1/60 s apart.
OJ_UNIT_TEST(FrameOptions, FrameRateCounter) {
    oj::render::FrameRateCounter counter;
    counter.frame(10.0);
    for (int i = 1; i < 30; ++i) counter.frame(10.0 + i / 60.0);
    check(counter.shown() == 0, "before half a second: 0");
    counter.frame(10.5);
    check(counter.shown() == 60, "half a second of 30 frames: 60");
    for (int i = 1; i <= 72; ++i) counter.frame(10.5 + i / 144.0);
    check(counter.shown() == 144, "then 72 frames in half a second: 144");
}

// MaxFPS without VSync, else the refresh rate above 60 (1000 at most); 0 the stock's 60.1; the interval + 0.1 as the
// stock's, none with -r.
OJ_UNIT_TEST(FrameOptions, FrameLimit) {
    using oj::render::frameRateLimit;
    check(frameRateLimit(true, 0, 60) == 0 && frameRateLimit(true, 0, 59) == 0 && frameRateLimit(true, 0, 0) == 0,
          "VSync at 60 Hz or less (or unknown): the stock's");
    check(frameRateLimit(true, 0, 144) == 144 && frameRateLimit(true, 90, 144) == 144, "VSync at 144 Hz: 144");
    check(frameRateLimit(false, 90, 144) == 90 && frameRateLimit(false, 0, 144) == 144 &&
              frameRateLimit(false, 5000, 60) == 1000,
          "no VSync: MaxFPS (1000 at most), else the refresh rate");
    using oj::render::frameInterval;
    check(std::fabs(frameInterval(0, false) - 1.0 / 60.1) < 1e-12 &&
              std::fabs(frameInterval(144, false) - 1.0 / 144.1) < 1e-12 && frameInterval(144, true) == 0.0,
          "1 / 60.1, 1 / 144.1, none with -r");
}

// 1280 x 720 on 2560 x 1440: the whole window; 1024 x 768 on 1920 x 1080: 1440 x 1080 in the middle, or stretched over
// it all; 1920 x 1080 on 2560 x 1440 at 16:10's 2560 x 1600: bars above and below.
OJ_UNIT_TEST(FrameOptions, PicturePlace) {
    using oj::render::placePicture;
    const auto same = placePicture(2560, 1440, 1280, 720, false);
    check(same.x0 == 0 && same.y0 == 0 && same.x1 == 2560 && same.y1 == 1440, "the same shape: the whole window");
    const auto kept = placePicture(1920, 1080, 1024, 768, false);
    check(kept.x0 == 240 && kept.y0 == 0 && kept.x1 == 1680 && kept.y1 == 1080, "4:3 kept: 240 .. 1680");
    const auto stretched = placePicture(1920, 1080, 1024, 768, true);
    check(stretched.x0 == 0 && stretched.x1 == 1920 && stretched.y1 == 1080, "stretched: the whole window");
    const auto tall = placePicture(2560, 1600, 1920, 1080, false);
    check(tall.x0 == 0 && tall.y0 == 80 && tall.x1 == 2560 && tall.y1 == 1520, "16:9 on 16:10: 80 .. 1520");
}
