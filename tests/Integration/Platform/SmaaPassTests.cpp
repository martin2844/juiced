// Integration tests: the fixed build's SMAA (Render/D3D9 with DeviceDesc::smaa, JuicedConfigExt's Shared/SmaaPass) on a
// Direct3D 9 device: a hard edge of the view smoothed, the rest of the picture as it was; off, nothing. The
// anti-aliasing changed while the device runs (setAntialiasing).

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Render/RenderDevice.h"

#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// The adapter has shaders 3.0 (SMAA's).
bool shaders30() {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == nullptr) return false;
    D3DCAPS9 caps{};
    const bool ok = SUCCEEDED(d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
                    caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && caps.VertexShaderVersion >= D3DVS_VERSION(3, 0);
    d3d->Release();
    return ok;
}

// A 128 x 128 view, black with a white triangle over its diagonal, antialiased as the device is made (`made`) or as
// it is set after (`changed`): the pixels part white along the diagonal and those changed far from it (the corners);
// -1 without a frame.
struct Result {
    int partial = -1, farChanged = -1;
};

struct Antialiasing {
    uint32_t msaa = 0;
    bool smaa = false;
};

Result smoothed(Antialiasing made, std::optional<Antialiasing> changed = std::nullopt) {
    oj::test::HiddenWindow window(128, 128);
    oj::render::DeviceDesc desc = oj::test::realDesc(window.get());
    desc.msaa = made.msaa;
    desc.smaa = made.smaa;
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, desc, &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return {};
    if (changed) device->setAntialiasing(changed->msaa, changed->smaa);
    const oj::test::Bitmap frame = oj::test::captureFrame(*device, [&] {
        device->clear(0x3, 0xFF000000u, 1.0f, 0);
        const oj::render::DebugVertex triangle[3] = {{0.0f, 0.0f, 0xFFFFFFFFu}, {128.0f, 0.0f, 0xFFFFFFFFu},
                                                     {0.0f, 96.0f, 0xFFFFFFFFu}};
        device->drawDebugTriangles(triangle, 3);
        device->antialias();
    });
    if (frame.width != 128) return {};
    Result r{0, 0};
    for (int32_t y = 0; y < 128; ++y) {
        for (int32_t x = 0; x < 128; ++x) {
            const uint32_t green = (frame.pixel(x, y) >> 8) & 0xFF;
            const bool part = green > 16 && green < 240;
            // The diagonal x / 128 + y / 96 = 1; far from it: more than 6 pixels either way.
            const float across = (static_cast<float>(x) / 128.0f + static_cast<float>(y) / 96.0f - 1.0f) * 77.0f;
            if (across > 6.0f || across < -6.0f) {
                if (part || (across < 0.0f) != (green >= 240)) ++r.farChanged;
            } else if (part) {
                ++r.partial;
            }
        }
    }
    return r;
}

}  // namespace

// SMAA on: the diagonal's stairs blended (pixels part white along it), the picture far from it as drawn; off, a hard
// edge.
OJ_INTEGRATION_TEST(SmaaPass, EdgeSmoothed) {
    if (!shaders30()) return;
    const Result off = smoothed({});
    const Result on = smoothed({0, true});
    if (off.partial < 0 || on.partial < 0) return;
    check(off.partial == 0, "off: a hard edge (" + std::to_string(off.partial) + " pixels part white)");
    check(on.partial >= 60 && on.farChanged == 0, "SMAA: the edge smoothed (" + std::to_string(on.partial) +
                                                       " pixels part white), far from it nothing changed (" +
                                                       std::to_string(on.farChanged) + ")");
}

// The anti-aliasing changed while the device runs (the graphics options' ADVANCED page): SMAA turned on smooths as
// with it from the start; MSAA 4x (the device reset as the next frame begins) as with it from the start (the
// triangle's sides on the view's edges part covered too); both turned off, a hard edge again.
OJ_INTEGRATION_TEST(SmaaPass, ChangedWhileRunning) {
    if (!shaders30()) return;
    const Result start = smoothed({0, true});
    const Result smaa = smoothed({}, Antialiasing{0, true});
    const Result off = smoothed({4, true}, Antialiasing{});
    if (start.partial < 0 || smaa.partial < 0 || off.partial < 0) return;
    check(smaa.partial == start.partial && smaa.farChanged == 0,
          "SMAA turned on: as with it from the start (" + std::to_string(smaa.partial) + " pixels part white)");
    check(off.partial == 0 && off.farChanged == 0, "MSAA and SMAA turned off: a hard edge");
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    const auto probe = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    const std::vector<uint32_t> counts = probe != nullptr ? probe->qualityCaps().multisampleCounts
                                                          : std::vector<uint32_t>{};
    if (std::find(counts.begin(), counts.end(), 4u) == counts.end()) return;
    const Result msaa = smoothed({}, Antialiasing{4, false});
    const Result msaaStart = smoothed({4, false});
    check(msaa.partial > 0 && msaa.partial == msaaStart.partial && msaa.farChanged == msaaStart.farChanged,
          "MSAA 4x turned on: the edge blended as with it from the start (" + std::to_string(msaa.partial) +
              " pixels part white)");
}
