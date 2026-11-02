#pragma once
// The fixed build's cars' shadows from the street lamps by night (WORLD LIGHT ULTRA, docs/fixes.md Added; the user,
// 2026-10-28: ULTRA by night as HI): the lamps' light is baked into the world, the cars move through it. The street
// lamps nearest a point ahead of the first player's camera (lampShadowCentre; TrackScene::lampsWithin, within
// kLampShadowReach, kLampShadows at most, each by its distance's fade past kLampShadowFull and its lead over the
// nearest left out, kLampShadowBand: the user, 2026-10-28, the shadows came and went at once) each draw the cars alone
// into a float map from the lamp looking down (JuicedConfigExt's HeadlightShadow.hlsl: the depth along the look over
// kLampShadowRange); the device's pass
// (IRenderDevice::lampShadows, JuicedConfigExt's LampShadowPass) takes each pixel of the view's depth of the ambient
// occlusion to the world and into the maps, and where a car hides a lamp multiplies the view by 1 - the lamp's share of
// the light there (kLampShadowStrength at its foot, the rest the ambient; away from it a point light's on level ground,
// (h / d)^3 of the foot's, over that plus the ambient). The cars themselves take none (their pixels marked in that
// depth, Renderer3d::kCarMark: the lamps light them by their own shaders).

#include "Core/Base/Matrix.h"
#include "Game/Scene/DepthShaders.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <vector>

namespace oj::scene {

class TrackScene;

constexpr size_t kLampShadows = 4;
constexpr float kLampShadowReach = 3000.0f;   // the lamps within 30 m of the centre
constexpr float kLampShadowFull = 2200.0f;    // fading out past 22 m
constexpr float kLampShadowBand = 500.0f;     // and within 5 m of the nearest left out
constexpr float kLampShadowAhead = 1200.0f;   // the centre 12 m ahead of the camera (a chase camera's car and beyond)
constexpr float kLampShadowRange = 2500.0f;   // a map's depth's 1: 25 m below the lamp
constexpr float kLampShadowTan = 1.6f;        // the half angle's tangent of a lamp's look (58 degrees)
constexpr float kLampShadowStrength = 0.5f;   // a lamp's share of the light at its foot (the rest the ambient; the
                                              // shadow's own depth the square of it: the user, 2026-10-29, the cars'
                                              // shadows from the lamps looked like dark bands, the lamps lighting the
                                              // road from both sides)
constexpr float kLampShadowBias = 0.004f;     // of the range: 10 cm
constexpr uint32_t kLampShadowSize = 1024;    // 3 cm a texel on the ground under a lamp

// A lamp's camera: at its place looking straight down, the view's up along the world's z.
Mat34 lampFrame(const Vec4& place);
// The point the lamps are looked for about: kLampShadowAhead along the look of the camera of world matrix `camera`.
Vec4 lampShadowCentre(const Mat34& camera);
// The pass's view of a camera (its half angles, the view to the world by its rows) and of the lamps (w their shadows'
// shares; their maps' cameras' world to clip rows, places and looks).
render::LampShadowView lampShadowView(render::Camera& camera, const std::vector<Vec4>& lamps,
                                      const std::vector<render::Camera*>& cameras);

class LampShadows {
public:
    explicit LampShadows(render::IRenderDevice& device);
    ~LampShadows();
    LampShadows(const LampShadows&) = delete;
    LampShadows& operator=(const LampShadows&) = delete;

    // The adapter takes float targets and the depth pass's shaders were made.
    bool usable() const { return m_shaders.usable(); }
    // The cars drawn into the maps of `lamps` (kLampShadows at most); the lamps drawn, in order.
    size_t draw(TrackScene& scene, const std::vector<Vec4>& lamps);
    // The view of the lamps last drawn for a view's camera.
    render::LampShadowView view(render::Camera& camera);
    // The maps of the lamps last drawn (0 past them).
    const std::array<render::TextureId, kLampShadows>& maps() const { return m_maps; }

private:
    render::IRenderDevice& m_device;
    DepthShaders m_shaders;
    std::array<render::TextureId, kLampShadows> m_maps{};
    std::array<render::Camera, kLampShadows> m_cameras;
    std::vector<Vec4> m_lamps;
};

}  // namespace oj::scene
