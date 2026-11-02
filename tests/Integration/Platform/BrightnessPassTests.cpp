// Integration tests: the fixed build's brightness over each finished frame (Render/D3D9 with DeviceDesc::brightness,
// JuicedConfigExt's Shared/BrightnessPass) on a Direct3D 9 device: the pass puts the game's states back, so the next
// frame draws as without it.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/RenderDevice.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// The pixels of the middle row part red (blended, neither 0 nor full) of a 4 x 4 picture, the left half red and the
// right half black, drawn 128 pixels wide with the sampler states the frame before set (linear): -1 without a frame.
// The frame before ends with the brightness `brightness` on a device with the pass `pass`.
int blendedAfter(bool pass, float brightness) {
    oj::test::HiddenWindow window(128, 128);
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.brightness = pass;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return -1;
    std::vector<uint32_t> texels(16);
    for (size_t i = 0; i < texels.size(); ++i) texels[i] = i % 4 < 2 ? 0xFFFF0000u : 0xFF000000u;
    oj::render::TextureData data;
    data.width = 4;
    data.height = 4;
    data.pixels = reinterpret_cast<const uint8_t*>(texels.data());
    data.size = texels.size() * 4;
    const oj::render::TextureId picture = device->createTexture(data);
    using oj::render::SamplerState;

    // The frame before: the sampler states set (clamp, linear), the brightness over it as it ends.
    device->setBrightness(brightness);
    if (device->beginFrame()) {
        device->clear(0x1, 0xFF808080u, 1.0f, 0);
        device->setSamplerState(0, SamplerState::AddressU, 3);  // clamp
        device->setSamplerState(0, SamplerState::AddressV, 3);
        device->setSamplerState(0, SamplerState::MinFilter, 2);  // linear
        device->setSamplerState(0, SamplerState::MagFilter, 2);
        device->setSamplerState(0, SamplerState::MipFilter, 0);  // none
        device->endFrame();
    }
    device->setBrightness(0.5f);
    const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
        device->clear(0x1, 0xFF000000u, 1.0f, 0);
        const oj::render::QuadVertex corners[4] = {{0.0f, 0.0f, 0.0f, 0.0f}, {128.0f, 0.0f, 1.0f, 0.0f},
                                                   {0.0f, 128.0f, 0.0f, 1.0f}, {128.0f, 128.0f, 1.0f, 1.0f}};
        device->drawQuad(corners, picture, oj::render::Blend::Alpha, true);
    });
    device->destroyTexture(picture);
    if (frame.width != 128) return -1;
    int blended = 0;
    for (int32_t x = 0; x < 128; ++x) {
        const uint32_t red = (frame.pixel(x, 64) >> 16) & 0xFF;
        if (red > 16 && red < 240) ++blended;
    }
    return blended;
}

}  // namespace

// The brightest brightness over a frame (the pass draws with point filtering): the next frame's picture still drawn
// with the linear filtering its game set, as on a device without the pass.
OJ_INTEGRATION_TEST(Brightness, GameStatesKept) {
    const int without = blendedAfter(false, 1.0f);
    const int with = blendedAfter(true, 1.0f);
    if (without < 0 || with < 0) return;
    check(without > 0, "without the pass: the edge blended (" + std::to_string(without) + " pixels)");
    check(with == without, "after the pass: the same edge (" + std::to_string(with) + " pixels)");
}
