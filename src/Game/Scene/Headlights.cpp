#include "Game/Scene/Headlights.h"

#include "Render/Program.h"
#include "Render/RenderDevice.h"


namespace oj::scene {

HeadlightFrame headlightFrame(const CarHeadlights& car) {
    const Mat34& root = car.root;
    std::array<float, 3> forward = {root[2], root[6], root[10]};
    HeadlightFrame f;
    f.side = {root[0], root[4], root[8]};
    std::array<float, 3> up = {root[1], root[5], root[9]};
    HeadlightBeam::Normalize(forward.data());
    HeadlightBeam::Normalize(f.side.data());
    HeadlightBeam::Normalize(up.data());
    HeadlightBeam::Axes(forward.data(), up.data(), f.beam.data(), f.up.data());
    f.place = {(car.right[3] + car.left[3]) * 0.5f, (car.right[7] + car.left[7]) * 0.5f,
               (car.right[11] + car.left[11]) * 0.5f};
    return f;
}

HeadlightConstants headlightConstants(const std::vector<CarHeadlights>& cars, float strength,
                                      const std::array<float, kHeadlights>* texels) {
    HeadlightConstants out;
    for (size_t i = 0; i < 4; ++i) out.constants[0][i] = HeadlightBeam::kColour[i];
    const float intensity = HeadlightBeam::kIntensity * strength;
    for (const CarHeadlights& car : cars) {
        if (out.count == kHeadlights) break;
        if (!car.shown) continue;
        const HeadlightFrame f = headlightFrame(car);
        const size_t light = static_cast<size_t>(out.count);
        float values[4][4];
        HeadlightBeam::Light(f.place.data(), f.beam.data(), f.side.data(), f.up.data(), intensity * car.level,
                             texels != nullptr ? (*texels)[light] : 0.0f, values);
        for (size_t k = 0; k < 4; ++k) {
            for (size_t i = 0; i < 4; ++i) out.constants[1 + 4 * light + k][i] = values[k][i];
        }
        out.on[light] = 1;
        ++out.count;
    }
    return out;
}

bool setHeadlights(render::IRenderDevice& device, const std::vector<CarHeadlights>& cars, float strength,
                   bool wereSet, const std::array<float, kHeadlights>* texels) {
    if (strength <= 0.0f) {
        if (wereSet) {
            const std::array<int32_t, kHeadlights> off{};
            device.setPixelShaderConstantB(kHeadlightBool, off.data(), kHeadlights);
        }
        return false;
    }
    const HeadlightConstants c = headlightConstants(cars, strength, texels);
    if (c.count > 0) {
        device.setPixelShaderConstantF(kHeadlightConstant, c.constants[0].data(),
                                       static_cast<uint32_t>(c.constants.size()));
    }
    device.setPixelShaderConstantB(kHeadlightBool, c.on.data(), kHeadlights);
    return true;
}

void setCarHeadlights(render::ProgramContext& programs, const std::vector<CarHeadlights>& cars, float strength,
                      bool lit) {
    HeadlightConstants c;
    if (lit && strength > 0.0f) c = headlightConstants(cars, strength);
    programs.setShared("CarHeadlight", c.constants[0].data(), sizeof c.constants);
}


}  // namespace oj::scene
