#pragma once
// The fixed build's sun shadows (WORLD LIGHT ULTRA, docs/fixes.md Added; the user, 2026-10-28: dynamic shadows as in
// the games of now, the baked ones gone): by day the sun's visibility from four cascaded shadow maps of the world, the
// scene objects and the cars about the view takes the place of the light tiles' baked sun shadow in the day shaders of
// the road and of Standard Opaque (JuicedConfigExt's Shared/Shaders/WorldLight.py, kind S: b9, c176 .. c192, s14) and
// gives the trees' leaves their sun (Trees.fx). The cascades reach 8, 25, 70 and 250 m along the view (the first's
// texel under 1 cm, a third of the 20 m cascade's: the user, 2026-10-28, the shadows about the car pixelated; the
// others as fine as the three of 20, 70 and 250 m were), each a sphere about its slice of the view kept to whole texels of the sun's view (the shadows
// do not crawl as the camera moves), the four in one float atlas of 4096 x 4096 (cells of 2048: the depth along the
// sun of what it sees first, DepthShaders: the alpha tested leaves cut out, the trees swayed); past the last the tiles
// stay.

#include "Core/Base/Matrix.h"
#include "Game/Scene/DepthShaders.h"
#include "Render/Camera.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>

namespace oj::scene {

class TrackScene;

constexpr int kSunCascades = 4;
constexpr std::array<float, kSunCascades> kSunCascadeEnds = {800.0f, 2500.0f, 7000.0f, 25000.0f};  // along the view
constexpr uint32_t kSunAtlas = 4096;
constexpr uint32_t kSunCell = 2048;
constexpr float kSunDepthScale = 100000.0f;  // the maps' depth: the distance along the sun / this
constexpr float kSunBackOff = 30000.0f;      // the sun's camera this far back from a cascade's middle (casters above)
constexpr float kSunBias = 10.0f;            // the depth's bias
constexpr float kSunNormalOffset = 0.002f;   // the lookup off the surface per unit of its distance from the view
constexpr float kSunNearStep = 2.0f;         // the first cascade's step between the soft edge's tests, in texels
// The first cascade widened about its casters by a texel (IRenderDevice::dilateDepth) where a neighbour is nearer the
// sun by more than this (of the maps' depth: 30 cm): its tests two texels apart stepped over a wire a texel wide, its
// shadow dotted near the view (the user, 2026-10-28), while a slope keeps its own depth.
constexpr float kSunDilateGap = 30.0f / kSunDepthScale;
constexpr uint32_t kSunBool = 9;
constexpr uint32_t kSunConstant = 176;       // c176 .. c192
constexpr uint32_t kSunConstants = 17;

// A cascade: its frame (side, up, the sun's direction and its middle, as rows) and its half size.
struct SunCascade {
    Mat34 frame{};
    float half = 0.0f;
};

// The cascades of a view (its world matrix, its half angles' tangents across and up, its near plane) for the sun's
// direction (the way its light goes): each slice's sphere, its middle kept to whole texels across the sun.
std::array<SunCascade, kSunCascades> sunCascades(const Mat34& view, float tanX, float tanY, float nearPlane,
                                                 const Vec4& sun);
// The atlas's cell centre of cascade i (u, v), half a texel in (the targets' pixel centres).
std::array<float, 2> sunCell(int cascade);
// The pixel shaders' constants c176 .. c192 for the cascades and the view's place: c176 + 3i .. c178 + 3i cascade i's
// rows (the world place to x and y across it and its depth), c188 the atlas's texel, the depth's bias, the atlas's
// size, half its texel, c189 and c190 the cells' centres of cascades 0 and 1, 2 and 3, c191 the view's place and the
// normal's offset, c192 the atlas's size and -0.5 (an instruction reads one constant register), the first cascade's
// step between the soft edge's tests and 1 / it (the tests of a box of 3 x 3 steps about the place).
std::array<float, 4 * kSunConstants> sunConstants(const std::array<SunCascade, kSunCascades>& cascades,
                                                  const Vec4& eye);

class SunShadows {
public:
    explicit SunShadows(render::IRenderDevice& device);
    ~SunShadows();
    SunShadows(const SunShadows&) = delete;
    SunShadows& operator=(const SunShadows&) = delete;

    // The adapter takes float targets and the shaders were made.
    bool usable() const { return m_shaders.usable(); }
    // The cascades drawn about `view` (the scene's casters seen from it), then the atlas on s14 and the constants set
    // (b9 on); false when they cannot be drawn. With `turns` (one view) the two far cascades are drawn by turns, one a
    // frame, the other kept with its frame of the frame before (the user, 2026-10-28: the shadow maps' drawing is the
    // ULTRA's cost; past 25 m a frame's lag shows nothing): only when the frame before drew them for the same view.
    bool draw(TrackScene& scene, render::Camera& view, const Vec4& sun, bool turns = false);
    // The atlas off its sampler.
    void unbind();
    // The atlas (for the debug builds' --sun-map).
    render::TextureId atlas() const { return m_atlas; }

private:
    render::IRenderDevice& m_device;
    DepthShaders m_shaders;
    render::TextureId m_atlas = 0;
    render::Camera m_camera;
    // The cascades as the atlas holds them; the draws counted (the far cascades' turns).
    std::array<SunCascade, kSunCascades> m_drawn{};
    uint64_t m_draws = 0;
};

}  // namespace oj::scene
