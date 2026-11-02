// Integration tests: the procedural clouds' map on a Direct3D 9 device (Render/D3D9, JuicedConfigExt's
// Shared/CloudPass): a clear sky's map all the clear light, an overcast one all the cloud's, half a cover some of each;
// the volume's map alike, its main view's clouds made and made again over the last frame's.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <filesystem>
#include <string>

namespace {

using oj::test::check;

bool shaders30() {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == nullptr) return false;
    D3DCAPS9 caps{};
    const bool ok = SUCCEEDED(d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
                    caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && caps.VertexShaderVersion >= D3DVS_VERSION(3, 0);
    d3d->Release();
    return ok;
}

// The map's greens: the least, the most and the mean.
struct Greens {
    uint32_t least = 255, most = 0;
    double mean = 0.0;
};

Greens mapGreens(oj::render::IRenderDevice& device, oj::render::TextureId map, float cover, bool volume = false) {
    oj::render::CloudView view;
    view.cover = cover;
    view.volume = volume;
    view.overcast = cover >= 1.0f ? 1.0f : 0.0f;
    view.clearLight = 0.6f;
    view.cloudLight = 0.1f;
    Greens g;
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "oj-tests-clouds.bmp";
    std::filesystem::remove(file);
    if (!device.beginFrame()) return g;
    const bool drawn = device.cloudMap(map, view);
    const bool captured = drawn && device.captureTarget(map, file.wstring());
    device.endFrame();
    const oj::test::Bitmap bmp = captured ? oj::test::readBitmap(file) : oj::test::Bitmap{};
    std::filesystem::remove(file);
    for (int32_t y = 0; y < bmp.height; ++y) {
        for (int32_t x = 0; x < bmp.width; ++x) {
            const uint32_t green = (bmp.pixel(x, y) >> 8) & 0xFFu;
            g.least = std::min(g.least, green);
            g.most = std::max(g.most, green);
            g.mean += green;
        }
    }
    if (bmp.width > 0 && bmp.height > 0) g.mean /= static_cast<double>(bmp.width) * bmp.height;
    return g;
}

}  // namespace

OJ_INTEGRATION_TEST(Clouds, OnARealDevice) {
    if (!shaders30()) return;
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    const oj::render::TextureId map = device->createRenderTarget(64, 64, false);
    check(map != 0, "the map's target");
    if (map == 0) return;
    const Greens clear = mapGreens(*device, map, 0.0f);
    const Greens overcast = mapGreens(*device, map, 1.0f);
    const Greens half = mapGreens(*device, map, 0.5f);
    check(clear.least >= 150 && clear.most <= 156, "a clear sky: the clear light (0.6) all over (" +
                                                       std::to_string(clear.least) + " .. " +
                                                       std::to_string(clear.most) + ")");
    check(overcast.least <= 30 && overcast.mean < 60.0, "an overcast sky: mostly the cloud's light (0.1), mean " +
                                                             std::to_string(overcast.mean));
    check(half.least <= 40 && half.most >= 150, "half a cover: clouds and clear sky");
    device->destroyTexture(map);
}

OJ_INTEGRATION_TEST(Clouds, VolumeOnARealDevice) {
    if (!shaders30()) return;
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    const oj::render::TextureId map = device->createRenderTarget(64, 64, false);
    check(map != 0, "the map's target");
    if (map == 0) return;
    const Greens clear = mapGreens(*device, map, 0.0f, true);
    const Greens overcast = mapGreens(*device, map, 1.0f, true);
    check(clear.least >= 150 && clear.most <= 156, "the volume's clear sky: the clear light all over (" +
                                                       std::to_string(clear.least) + " .. " +
                                                       std::to_string(clear.most) + ")");
    check(overcast.mean < 90.0, "the volume's overcast sky: its shadow, mean " + std::to_string(overcast.mean));
    // The main view (looking along z): made, then made again over the last frame's a moment later.
    oj::render::CloudView view;
    view.volume = true;
    view.cover = 0.5f;
    bool first = false, again = false;
    if (device->beginFrame()) {
        first = device->cloudView(view);
        device->endFrame();
    }
    view.seconds = 0.02f;
    if (device->beginFrame()) {
        again = device->cloudView(view);
        device->endFrame();
    }
    check(first && again, "the main view's clouds made, then over the last frame's");
    device->destroyTexture(map);
}
