// Integration tests: the renderer options' texture filter (Render/FrameOptions TextureQuality, the registry's
// TextureFilter) and the UI's pictures under it: their transparent texels coloured as their seen neighbours
// (bleedTransparent), the HUD's dial of the game's files and a picture's edge drawn on a Direct3D 9 device.

#include "Framework/Fakes/NullRenderDevice.h"
#include "Framework/Fixtures.h"
#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/Language.h"
#include "Core/Formats/Ddt.h"
#include "Game/UI/UiScreen.h"
#include "Render/FrameOptions.h"
#include "Render/RenderDevice.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// 4 x 4 texels: the left half opaque red, the right half transparent white (a HUD picture's edge).
std::vector<uint32_t> redAndClear() {
    std::vector<uint32_t> texels(16);
    for (size_t i = 0; i < texels.size(); ++i) texels[i] = i % 4 < 2 ? 0xFFFF0000u : 0x00FFFFFFu;
    return texels;
}

// The pictures the screen loads, their first level kept.
class KeepingDevice : public oj::test::NullRenderDevice {
public:
    oj::render::TextureId createTexture(const oj::render::TextureData& data) override {
        const size_t bytes = static_cast<size_t>(data.width) * data.height * 4;
        last.assign(data.width * data.height, 0);
        if (data.pixels != nullptr && data.size >= bytes) std::memcpy(last.data(), data.pixels, bytes);
        return NullRenderDevice::createTexture(data);
    }
    std::vector<uint32_t> last;
};

// The middle row of the 4 x 4 picture drawn 128 pixels wide over black as a 2D quad with a picture's sampler (point,
// no mips): the pixels with green or blue (white taken into the red edge) and those part red (blended, neither 0 nor
// full); -1 without a frame.
struct Edge {
    int white = -1, blended = -1;
};

// `madeWith`: the device's texture filter as it is made, then `textureFilter` set while it runs.
Edge edgeOf(uint32_t textureFilter, const std::vector<uint32_t>& texels, uint32_t madeWith) {
    oj::test::HiddenWindow window(128, 128);
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.textureFilter = madeWith;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return {};
    oj::render::TextureData data;
    data.width = 4;
    data.height = 4;
    data.pixels = reinterpret_cast<const uint8_t*>(texels.data());
    data.size = texels.size() * 4;
    const oj::render::TextureId picture = device->createTexture(data);
    using oj::render::SamplerState;
    device->setSamplerState(0, SamplerState::AddressU, 3);  // clamp
    device->setSamplerState(0, SamplerState::AddressV, 3);
    device->setSamplerState(0, SamplerState::MinFilter, 1);  // point
    device->setSamplerState(0, SamplerState::MagFilter, 1);
    device->setSamplerState(0, SamplerState::MipFilter, 0);  // none
    device->setTextureFilter(textureFilter);
    const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
        device->clear(0x1, 0xFF000000u, 1.0f, 0);
        const oj::render::QuadVertex corners[4] = {{0.0f, 0.0f, 0.0f, 0.0f}, {128.0f, 0.0f, 1.0f, 0.0f},
                                                   {0.0f, 128.0f, 0.0f, 1.0f}, {128.0f, 128.0f, 1.0f, 1.0f}};
        device->drawQuad(corners, picture, oj::render::Blend::Alpha, true);
    });
    device->destroyTexture(picture);
    if (frame.width != 128) return {};
    Edge out{0, 0};
    for (int32_t x = 0; x < 128; ++x) {
        const uint32_t c = frame.pixel(x, 64);
        const uint32_t red = (c >> 16) & 0xFF;
        if (((c >> 8) & 0xFF) > 16 || (c & 0xFF) > 16) ++out.white;
        if (red > 16 && red < 240) ++out.blended;
    }
    return out;
}

}  // namespace

// The texels of alpha 0 take their seen neighbours' mean colour outwards, their alpha kept; the seen ones and a picture
// with none seen stay as they were.
OJ_UNIT_TEST(TextureFilter, BleedTransparent) {
    std::vector<uint32_t> texels = redAndClear();
    texels[1] = 0x800000FFu;  // a half seen blue beside the red
    oj::render::bleedTransparent(texels.data(), 4, 4);
    bool kept = texels[0] == 0xFFFF0000u && texels[1] == 0x800000FFu && texels[4] == 0xFFFF0000u;
    bool coloured = true;
    for (size_t i = 0; i < texels.size(); ++i) {
        if (i % 4 < 2) continue;
        coloured = coloured && (texels[i] >> 24) == 0 && (texels[i] & 0x00FFFFFFu) != 0x00FFFFFFu;
    }
    // The first row's third texel: the mean of its two seen neighbours, the half seen blue and the red below it on
    // the left.
    coloured = coloured && texels[2] == 0x007F007Fu;
    std::vector<uint32_t> clear(16, 0x00FFFFFFu);
    oj::render::bleedTransparent(clear.data(), 4, 4);
    for (const uint32_t c : clear) kept = kept && c == 0x00FFFFFFu;
    check(kept, "the seen texels and a picture without any as they were");
    check(coloured, "the transparent texels coloured as their seen neighbours, alpha 0 kept");
}

// The HUD's dial (ui_dial.ddt: 38027 of its 65536 texels transparent white) as the UI loads it, the texture filter on
// or off (the graphics options turn it on while the game runs): each transparent texel beside seen ones takes their
// mean colour (the first pass), the seen ones and every alpha as the file has them.
OJ_INTEGRATION_TEST(TextureFilter, HudPictureBled) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::Language& language = *oj::Language::byName("english");
    KeepingDevice filtered;
    oj::ui::Screen screen(filtered, language);
    const auto bytes = oj::GameFiles(std::filesystem::current_path()).read("ui/textures/ui_dial.ddt");
    oj::Ddt file;
    const bool loaded = bytes && file.parse(*bytes) && file.pixels.size() >= 65536 * 4 &&
                        screen.loadTexture("ui_dial") != 0 && filtered.last.size() == 65536;
    check(loaded, "ui_dial.ddt loaded (256 x 256)");
    if (!loaded) return;
    std::vector<uint32_t> plain(65536);
    std::memcpy(plain.data(), file.pixels.data(), 65536 * 4);
    size_t clearWhite = 0, seenChanged = 0, alphaChanged = 0, beside = 0, notTheMean = 0;
    for (size_t i = 0; i < 65536; ++i) {
        const uint32_t was = plain[i], now = filtered.last[i];
        if ((was >> 24) == 0 && (was & 0x00FFFFFFu) == 0x00FFFFFFu) ++clearWhite;
        if ((was >> 24) != 0 && was != now) ++seenChanged;
        if ((was >> 24) != (now >> 24)) ++alphaChanged;
        if ((was >> 24) != 0) continue;
        const int x = static_cast<int>(i % 256), y = static_cast<int>(i / 256);
        uint32_t r = 0, g = 0, b = 0, n = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx, ny = y + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx > 255 || ny > 255) continue;
                const uint32_t c = plain[static_cast<size_t>(ny) * 256 + static_cast<size_t>(nx)];
                if ((c >> 24) == 0) continue;
                r += (c >> 16) & 0xFF;
                g += (c >> 8) & 0xFF;
                b += c & 0xFF;
                ++n;
            }
        }
        if (n == 0) continue;
        ++beside;
        if (now != (((r / n) << 16) | ((g / n) << 8) | (b / n))) ++notTheMean;
    }
    check(clearWhite == 38027, "the file's dial: 38027 transparent white texels (" + std::to_string(clearWhite) + ")");
    check(seenChanged == 0 && alphaChanged == 0, "the seen texels and every alpha as the file has them");
    check(beside > 0 && notTheMean == 0, "each of the " + std::to_string(beside) +
                                             " transparent texels beside seen ones their mean colour (" +
                                             std::to_string(notTheMean) + " not)");
}

// A picture's edge drawn as the UI draws it: point filtered without the texture filter (crisp); with it (16x) the
// edge blended, the transparent white taken into it as it is in the file (Quadra 2026-10-25: a white outline round the
// HUD's dials), none once the transparent texels are coloured as their neighbours. The filter turned on and off while
// the device runs (the graphics options' ADVANCED page) on the states the game set before.
OJ_INTEGRATION_TEST(TextureFilter, PictureEdge) {
    const std::vector<uint32_t> file = redAndClear();
    std::vector<uint32_t> bled = file;
    oj::render::bleedTransparent(bled.data(), 4, 4);
    const Edge off = edgeOf(0, file, 0);
    const Edge white = edgeOf(5, file, 5);
    const Edge clean = edgeOf(5, bled, 5);
    const Edge turnedOn = edgeOf(5, bled, 0);
    const Edge turnedOff = edgeOf(0, file, 5);
    if (off.white < 0 || white.white < 0 || clean.white < 0 || turnedOn.white < 0 || turnedOff.white < 0) return;
    check(off.white == 0 && off.blended == 0, "without the texture filter: the edge point filtered");
    check(turnedOn.white == 0 && turnedOn.blended == clean.blended,
          "the filter turned on while the device runs: the edge blended as with it from the start");
    check(turnedOff.white == 0 && turnedOff.blended == 0,
          "the filter turned off while the device runs: the game's point filtering back");
    check(white.white > 0, "with the texture filter, the file's texels: white in the edge (" +
                               std::to_string(white.white) + " pixels)");
    check(clean.white == 0 && clean.blended > 0, "with the texture filter, coloured texels: the edge blended (" +
                                                     std::to_string(clean.blended) + " pixels), no white in it");
}
