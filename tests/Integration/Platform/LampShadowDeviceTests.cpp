// Integration tests: WORLD LIGHT ULTRA's cars' shadows from the street lamps on a Direct3D 9 device (Render/D3D9,
// JuicedConfigExt's Shared/LampShadowPass): a lamp above the middle of a level view, its map clear (nothing between)
// leaves the view as it was; a map whose caster hides the lamp darkens the middle by the lamp's share of the light
// there; with no lamp the pass does not run.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Game/Scene/LampShadows.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <array>
#include <cmath>
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

uint32_t grey(uint32_t pixel) {
    return pixel & 0xFFu;
}

}  // namespace

OJ_INTEGRATION_TEST(LampShadows, OnARealDevice) {
    if (!shaders30()) return;
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    const oj::render::TextureId depth = device->createFloatTarget(64, 64);
    const oj::render::TextureId map = device->createFloatTarget(64, 64);
    check(depth != 0 && map != 0, "the depth's and the lamp's map's float targets");
    if (depth == 0 || map == 0) return;
    // The view: a camera at the world's origin looking along its z; the depth a wall half the range away. The lamp 9 m
    // above that wall's middle, looking down.
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(0.5) * 2.0), static_cast<float>(std::atan(0.5) * 2.0), false);
    camera.resetMatrices();
    camera.invalidate();
    camera.update();
    const float range = 1000.0f;
    const oj::Vec4 lamp{0.0f, 900.0f, range * 0.5f, 1.0f};
    oj::render::Camera lampView;
    const auto fov = static_cast<float>(std::atan(oj::scene::kLampShadowTan) * 2.0);
    lampView.setFov(fov, fov, false);
    lampView.setNear(10.0f);
    lampView.setFar(oj::scene::kLampShadowRange);
    lampView.local = oj::scene::lampFrame(lamp);
    lampView.invalidate();
    lampView.update();
    oj::render::LampShadowView view = oj::scene::lampShadowView(camera, {lamp}, {&lampView});
    view.range = range;
    const std::array<oj::render::TextureId, 4> maps{map, 0, 0, 0};
    // The depth's value, the map's (1: nothing before the far end; 0: a caster at the lamp), the view's middle after.
    const auto run = [&](uint32_t depthColour, uint32_t mapColour, bool& ran) {
        device->setRenderTarget(depth, 0);
        device->clear(oj::render::ClearFlags::Target, depthColour, 1.0f, 0);
        device->setRenderTarget(map, 0);
        device->clear(oj::render::ClearFlags::Target, mapColour, 1.0f, 0);
        device->setRenderTarget(0, 0);
        const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
            device->clear(0x3, 0xFF808080u, 1.0f, 0);
            ran = device->lampShadows(depth, maps, view);
        });
        return frame.width == 128 ? grey(frame.pixel(64, 64)) : 0u;
    };
    bool ran = false;
    // A float target's clear colour: its red channel over 255 (0x80: about 0.5, the wall half the range away).
    const uint32_t lit = run(0xFF800000u, 0xFFFFFFFFu, ran);
    check(ran, "the pass ran");
    check(lit >= 0x7E && lit <= 0x82, "nothing between the lamp and the wall: the view as it was (" +
                                          std::to_string(lit) + ")");
    const uint32_t shadowed = run(0xFF800000u, 0xFF000000u, ran);
    // The lamp is straight above the view's middle, so its share of the light there is kLampShadowStrength and the view
    // keeps 1 - it (the user, 2026-10-29: 75 % read as hard dark bands on a car between two lamps, 50 % now).
    const uint32_t kept =
        static_cast<uint32_t>(128.0f * (1.0f - oj::scene::kLampShadowStrength));
    check(shadowed >= kept - 4 && shadowed <= kept + 4 && shadowed < lit - 16,
          "a caster hides the lamp: the lamp's share taken from the middle (" + std::to_string(shadowed) + ")");
    oj::render::LampShadowView none = view;
    none.count = 0;
    check(!device->lampShadows(depth, maps, none), "no lamp: the pass does not run");
}
