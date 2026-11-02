#pragma once
// The fixed build's ambient occlusion (WORLD LIGHT HI and ULTRA, docs/fixes.md Added; the user, 2026-10-28: a deeper
// world): the view's depth drawn from its camera into a float target of half its size (the opaque meshes with the
// ground's and the alpha tested ones cut out, the trees swayed: DepthShaders, Renderer3d::shadowShaders with `ground`;
// the depth along the view over kOcclusionRange), then the device's pass (IRenderDevice::ambientOcclusion, JuicedConfigExt's
// AmbientOcclusionPass) multiplies the view by the occlusion after its opaque parts (TrackScene::draw).

#include "Game/Scene/DepthShaders.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>

namespace oj::scene {

class TrackScene;

constexpr float kOcclusionRange = 30000.0f;    // the depth's 1: 300 m
constexpr float kOcclusionRadius = 60.0f;      // the half sphere tested over each point: 60 cm
constexpr float kOcclusionStrength = 1.0f;
constexpr float kOcclusionBias = 4.0f;
constexpr float kOcclusionFadeFrom = 5000.0f;  // faded out from 50 m to 80 m
constexpr float kOcclusionFadeLength = 3000.0f;
// A tree's leaves (the user, 2026-10-28: the trees too light, no occlusion on them): their crown as a volume, a sphere
// of 2 m about each point; up to 55 % darker, a fifth of it everywhere, the rest where the crown hides the point,
// faded out from 60 m to 120 m.
constexpr float kFoliageRadius = 350.0f;
constexpr float kFoliageStrength = 0.6f;
constexpr float kFoliageFadeFrom = 6000.0f;
constexpr float kFoliageFadeLength = 6000.0f;

// The depth's size for a view: half of it either way, 16 at least.
uint32_t occlusionSize(uint32_t view);
// The rain's puddles (the road under them marked as the water, TrackScene::markGround): the depth's stencil bit (it
// uses none of it else), and the share of a place's way to the camera the ground is drawn nearer by, so that it lies in
// front of the water sheet it is marked under (the sheet floats over the road, a little under it in places).
inline constexpr uint32_t kPuddleBit = 0x40;
inline constexpr float kPuddleReach = 0.1f;
// `worldToClip` with each place first moved `reach` of its way toward `eye`: the same pixel, nearer.
Mat44 nearerToClip(const Mat44& worldToClip, const Vec4& eye, float reach);
// The pass's view of a camera: its half angles from its projection, the constants above.
render::AmbientOcclusionView occlusionView(const render::Camera& camera);
// WORLD LIGHT ULTRA by night (the user, 2026-10-28: ULTRA by night as HI): the view of the lights reflected in the
// road from the same depth: the camera's half angles, the world's up in its frame, the road wet (1) or dry (0); the
// rays reach kReflectionReach.
constexpr float kReflectionReach = 20000.0f;  // 200 m
render::LightReflectionView reflectionView(render::Camera& camera, float wetness);
// A street lamp's streak in the wet road seen from `camera` (the user, 2026-10-28: the flares' y reflections, quads of
// their texture, looked square): the lamp's light mirrored in the road under it (`ground` the road's height there), a
// soft line down the view from about the lamp's foot to its mirror point, then fading on toward the view over
// kStreakLength times that way; kStreakWidth across at the point (a rough road's spread), `colour` (a street lamp's
// warm white, kStreakColour; a car's light its own) at kStreakStrength times `strength`, faded out toward
// kStreakReach. The streak's eight numbers of the view
// (render::LightReflectionView's streaks); false when the lamp's foot is below the view, the streak off it either side,
// the point behind the camera or the streak faded out.
constexpr float kStreakWidth = 60.0f;      // half of it: 60 cm
constexpr float kStreakLength = 1.5f;
constexpr float kStreakStrength = 0.45f;
constexpr float kStreakReach = 15000.0f;   // 150 m
constexpr std::array<float, 3> kStreakColour{1.0f, 0.8f, 0.55f};
// The lamps given streaks (TrackScene::lampStreaks; the user, 2026-10-28: the glare on the road came and went at once
// about the camera): the 16 nearest a point kStreakAhead along the view's look of every lit flare with a y reflection,
// seen or not (the set changing only as one passes another), each by its lead over the nearest left out (rankFade over
// kStreakBand) and by its flare's visibility eased over kStreakEase seconds (a car passing before a lamp).
constexpr float kStreakAhead = 6000.0f;    // 60 m
constexpr float kStreakBand = 1000.0f;     // 10 m
constexpr float kStreakEase = 0.12f;
bool lampStreak(render::Camera& camera, const Vec4& lamp, float ground, float strength,
                const std::array<float, 3>& colour, std::array<float, 8>& out);

class AmbientOcclusion {
public:
    explicit AmbientOcclusion(render::IRenderDevice& device);
    ~AmbientOcclusion();
    AmbientOcclusion(const AmbientOcclusion&) = delete;
    AmbientOcclusion& operator=(const AmbientOcclusion&) = delete;

    // The depth pass's shaders were made.
    bool usable() const { return m_shaders.usable(); }
    // The view's depth of `camera` (a view of width x height): its target, 0 when it cannot be drawn. With `marks`
    // (the lights reflected in the road) the road under the rain's puddles drawn again marked as the water
    // (TrackScene::markGround).
    render::TextureId drawDepth(TrackScene& scene, render::Camera& camera, uint32_t width, uint32_t height,
                                bool marks = false);

private:
    render::IRenderDevice& m_device;
    DepthShaders m_shaders;
    render::TextureId m_target = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

}  // namespace oj::scene
