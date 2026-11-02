// Integration tests: REFLECTIONS ULTRA on a Direct3D 9 device (Render/D3D9, Game/Scene/TrackScene's cars' cube): a
// face of the cars' cube copied into the stock's smaller cube.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Game/Scene/Scene3d.h"
#include "Render/RenderDevice.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// Each face of a 256 cube into a 128 cube (scaled) and into one of its size; not into a flat target, not from none.
OJ_INTEGRATION_TEST(ReflectionsUltra, CopyCubeFace) {
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device = oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    const uint32_t size = oj::scene::Engine::kCarsCubeSize;
    const oj::render::TextureId cars = device->createRenderTarget(size, size, true);
    const oj::render::TextureId stock = device->createRenderTarget(128, 128, true);
    const oj::render::TextureId same = device->createRenderTarget(size, size, true);
    const oj::render::TextureId flat = device->createRenderTarget(128, 128, false);
    check(cars != 0 && stock != 0 && same != 0 && flat != 0, "the targets made");
    bool copied = true;
    for (uint32_t face = 0; face < 6; ++face) {
        device->setRenderTarget(cars, face);
        device->clear(oj::render::ClearFlags::Target | oj::render::ClearFlags::ZBuffer, 0xFF204060u, 1.0f, 0);
        copied = copied && device->copyCubeFace(cars, stock, face) && device->copyCubeFace(cars, same, face);
    }
    device->setRenderTarget(0, 0);
    check(copied, "each face into the 128 cube and the 256 one");
    check(!device->copyCubeFace(cars, flat, 0) && !device->copyCubeFace(0, stock, 0),
          "not into a flat target, not from none");
    for (const oj::render::TextureId id : {cars, stock, same, flat}) {
        if (id != 0) device->destroyTexture(id);
    }
}
