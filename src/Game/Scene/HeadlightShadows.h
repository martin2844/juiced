#pragma once
// The fixed build's headlights' shadows (HEADLIGHTS ULTRA, docs/fixes.md Added): what each light of Game/Scene/
// Headlights sees first, drawn from the light into a map of the distance along it (a float target, JuicedConfigExt's
// Shared/Shaders/HeadlightShadow.hlsl through Renderer3d::shadowShaders), which the night shaders' headlight blocks
// read (the samplers s8 + i, JuicedConfigExt's WorldLight.py). The maps' sizes by the car (the user, 2026-10-28, a
// map half as high as wide, the user's tuning 1.5 times the first): the first player's car 1536 x 768 each frame, the
// cars within kNearShadows of it 768 x 384 each frame, the others 384 x 192 drawn by turns (one frame in three each).

#include "Game/Scene/DepthShaders.h"
#include "Game/Scene/Headlights.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace oj::scene {

class CarModel;
class TrackScene;

enum class ShadowTier { Own, Near, Far };

constexpr float kNearShadows = 4000.0f;  // 40 m from the first player's car
constexpr uint32_t kFarShadowTurns = 3;  // a far map drawn one frame in three

struct ShadowSize {
    uint32_t width = 0;
    uint32_t height = 0;
};
ShadowSize shadowSize(ShadowTier tier);
// The tier of a car's map: the first player's own, near within kNearShadows of it, else far.
ShadowTier shadowTier(bool own, float distance);
// A map of the car drawn this frame: always but a far one's, drawn on its turn (by the car's index) or when it has
// none yet.
bool shadowDue(ShadowTier tier, uint32_t car, uint64_t frame, bool drawn);

class HeadlightShadows {
public:
    // A light of the frame, in the night shaders' order: the car's lights, its model (left out of its own map), its
    // index (its maps and its turn) and its tier.
    struct Light {
        CarHeadlights headlights;
        const CarModel* model = nullptr;
        uint32_t car = 0;
        ShadowTier tier = ShadowTier::Far;
    };

    explicit HeadlightShadows(render::IRenderDevice& device);
    ~HeadlightShadows();
    HeadlightShadows(const HeadlightShadows&) = delete;
    HeadlightShadows& operator=(const HeadlightShadows&) = delete;

    // The adapter takes float targets and the shaders were made.
    bool usable() const { return m_shaders.usable(); }
    // The maps due this frame drawn (the scene's casters from each light), then bound on s8 .. in the lights' order;
    // each light's texel width (0: no map) for its constants (headlightConstants).
    std::array<float, kHeadlights> draw(TrackScene& scene, const std::vector<Light>& lights, uint64_t frame);
    // The maps off the samplers.
    void unbind();
    // The maps made (for the tests): by the car and the tier.
    size_t mapsMade() const { return m_maps.size(); }
    // The map bound last for light i (0: none).
    render::TextureId boundMap(size_t i) const { return i < m_bound.size() ? m_bound[i] : 0; }

private:
    struct Map {
        render::TextureId id = 0;
        bool drawn = false;
    };

    render::IRenderDevice& m_device;
    DepthShaders m_shaders;
    std::map<std::pair<uint32_t, ShadowTier>, Map> m_maps;
    std::array<render::TextureId, kHeadlights> m_bound{};
    render::Camera m_camera;
};

}  // namespace oj::scene
