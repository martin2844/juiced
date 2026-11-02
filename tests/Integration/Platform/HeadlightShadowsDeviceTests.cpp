// Integration tests: HEADLIGHTS ULTRA on a Direct3D 9 device (Render/D3D9, Game/Scene/HeadlightShadows): the float
// targets of the maps, the depth pass's shaders, the shaders with the shadow taps made in the game's place.

#include "Framework/RealDevice.h"
#include "Framework/Test.h"

#include "Game/Scene/HeadlightShadows.h"
#include "Render/RenderDevice.h"

#include <array>
#include <string>

#include <Shaders/WorldLightShaders.h>

namespace {

using oj::test::check;

}  // namespace

// The maps' float targets of each size made (and again after a reset), the depth pass's shaders made, the maps bound
// on s8 .. and off again; the shaders with the headlights' shadow taps made by the device.
OJ_INTEGRATION_TEST(HeadlightShadows, OnARealDevice) {
    oj::test::HiddenWindow window(128, 128);
    std::string error;
    auto device =
        oj::render::createDevice(oj::render::Backend::D3D9, oj::test::realDesc(window.get()), &error);
    check(device != nullptr, "a Direct3D 9 device (the tests need one): " + error);
    if (device == nullptr) return;
    std::array<oj::render::TextureId, oj::scene::kHeadlights> maps{};
    bool made = true;
    for (const oj::scene::ShadowTier tier : {oj::scene::ShadowTier::Own, oj::scene::ShadowTier::Near,
                                             oj::scene::ShadowTier::Far}) {
        const oj::scene::ShadowSize size = oj::scene::shadowSize(tier);
        const oj::render::TextureId id = device->createFloatTarget(size.width, size.height);
        made = made && id != 0 && device->targetWidth(id) == size.width && device->targetHeight(id) == size.height;
        maps[static_cast<size_t>(tier)] = id;
    }
    check(made, "the maps' float targets of the three sizes");
    {
        oj::scene::HeadlightShadows shadows(*device);
        check(shadows.usable(), "the depth pass's shaders made");
    }
    device->setShadowMaps(maps.data(), oj::scene::kHeadlights);
    const std::array<oj::render::TextureId, oj::scene::kHeadlights> none{};
    device->setShadowMaps(none.data(), oj::scene::kHeadlights);
    for (const oj::render::TextureId id : maps) {
        if (id != 0) device->destroyTexture(id);
    }
    // The replacements of the headlights' feature (JuicedConfigExt's table: the night's road and Standard Opaque 3.0,
    // the day's road, the last two with the sun's too for a tunnel's day) made by the device.
    int created = 0, headlit = 0;
    for (const WorldLightShaders::Shader& s : WorldLightShaders::kShaders) {
        if ((s.features & WorldLightShaders::Headlights) == 0 || s.kind != WorldLightShaders::Kind::Pixel) continue;
        ++headlit;
        const oj::render::ShaderId id = device->createPixelShader(s.code);
        if (id != 0) {
            ++created;
            device->destroyShader(id);
        }
    }
    check(headlit == 5 && created == 5, "the 5 shaders with the shadow taps made (" + std::to_string(created) +
                                            " of " + std::to_string(headlit) + ")");
    // Every pixel shader of the table made (each within the 512 slots of the shaders 3.0, WorldLight.py checks).
    int pixel = 0, pixelMade = 0;
    for (const WorldLightShaders::Shader& s : WorldLightShaders::kShaders) {
        if (s.kind != WorldLightShaders::Kind::Pixel) continue;
        ++pixel;
        const oj::render::ShaderId id = device->createPixelShader(s.code);
        if (id != 0) {
            ++pixelMade;
            device->destroyShader(id);
        }
    }
    check(pixel > 0 && pixelMade == pixel, "every pixel shader of the table made (" + std::to_string(pixelMade) +
                                               " of " + std::to_string(pixel) + ")");
}
