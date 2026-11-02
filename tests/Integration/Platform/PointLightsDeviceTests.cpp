// Integration tests: point lights on the view's surfaces on a Direct3D 9 device (the cars' flames; Render/D3D9,
// JuicedConfigExt's Shared/PointLightPass): a light just before a wall lightens the wall's middle, its far corners
// little; with no light the pass does not run.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Game/Scene/FlameLights.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <cmath>
#include <string>
#include <vector>

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

}  // namespace

OJ_INTEGRATION_TEST(PointLights, OnARealDevice) {
    if (!shaders30()) return;
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    const oj::render::TextureId depth = device->createFloatTarget(64, 64);
    check(depth != 0, "the depth's float target");
    if (depth == 0) return;
    // A camera at the origin looking along z at a wall 5 m away (the depth half of a range of 10 m); a light 1 m before
    // the wall's middle.
    oj::render::Camera camera;
    camera.setFov(static_cast<float>(std::atan(1.0) * 2.0), static_cast<float>(std::atan(1.0) * 2.0), false);
    camera.resetMatrices();
    camera.invalidate();
    camera.update();
    oj::render::PointLight light;
    light.place = {0.0f, 0.0f, 400.0f};
    light.reach = 400.0f;
    light.falloff = 200.0f;
    light.colour = {1.0f, 1.0f, 1.0f};
    oj::render::PointLightView view = oj::scene::flameView(camera, {light});
    view.range = 1000.0f;
    device->setRenderTarget(depth, 0);
    device->clear(oj::render::ClearFlags::Target, 0xFF800000u, 1.0f, 0);  // 0.5 everywhere: the wall
    device->setRenderTarget(0, 0);
    bool ran = false;
    const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
        device->clear(0x3, 0xFF202020u, 1.0f, 0);
        ran = device->pointLights(depth, view);
    });
    check(ran, "the pass ran");
    if (frame.width != 128) return;
    const uint32_t middle = frame.pixel(64, 64) & 0xFFu, corner = frame.pixel(2, 2) & 0xFFu;
    check(middle > 0xA0 && corner < 0x30, "the light lightens the wall's middle (" + std::to_string(middle) +
                                              "), a far corner little (" + std::to_string(corner) + ")");
    oj::render::PointLightView none = view;
    none.count = 0;
    check(!device->pointLights(depth, none), "no light: the pass does not run");
}
