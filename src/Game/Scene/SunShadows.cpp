#include "Game/Scene/SunShadows.h"

#include "Core/Base/Log.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Game/Scene/TrackScene.h"
#include "Render/Renderer3d.h"

#include <algorithm>
#include <cmath>

namespace oj::scene {

namespace {

constexpr uint32_t kCullNone = 1;  // D3DCULL_NONE: both sides of the casters

using Vec3 = std::array<float, 3>;

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

float dot(const Vec3& a, const Vec3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vec3 normalized(const Vec3& v) {
    const float length = std::sqrt(dot(v, v));
    return length > 1e-6f ? Vec3{v[0] / length, v[1] / length, v[2] / length} : Vec3{0.0f, -1.0f, 0.0f};
}

Vec3 column(const Mat34& m, int c) {
    return {m[c], m[4 + c], m[8 + c]};
}

}  // namespace

std::array<SunCascade, kSunCascades> sunCascades(const Mat34& view, float tanX, float tanY, float nearPlane,
                                                 const Vec4& sunDirection) {
    const Vec3 sun = normalized({sunDirection[0], sunDirection[1], sunDirection[2]});
    // The sun's frame: its side across the world's up (the world's x when the sun stands straight above).
    const Vec3 reference = std::fabs(sun[1]) > 0.99f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 side = normalized(cross(reference, sun));
    const Vec3 up = cross(sun, side);
    const Vec3 eye = column(view, 3);
    const Vec3 forward = normalized(column(view, 2));
    const float k2 = tanX * tanX + tanY * tanY;
    std::array<SunCascade, kSunCascades> out{};
    float from = nearPlane;
    for (int i = 0; i < kSunCascades; ++i) {
        const float to = kSunCascadeEnds[static_cast<size_t>(i)];
        // The slice's sphere: its middle along the view where the near and the far corners are as far, at most the far
        // plane; its radius to whole metres (a size that stays as the view turns).
        const float middle = std::min(0.5f * (from + to) * (1.0f + k2), to);
        const float farCorner = std::sqrt((to - middle) * (to - middle) + to * to * k2);
        const float nearCorner = std::sqrt((middle - from) * (middle - from) + from * from * k2);
        const float half = std::ceil(std::max(farCorner, nearCorner) / 100.0f) * 100.0f;
        const Vec3 centre = {eye[0] + forward[0] * middle, eye[1] + forward[1] * middle,
                             eye[2] + forward[2] * middle};
        // Kept to whole texels across the sun; the first cascade to whole steps of its soft edge's tests (kSunNearStep
        // texels: kept to texels, a camera's shake moved it a texel back and forth, its tests two texels apart then
        // took the other texels, the user 2026-10-28: a wire's shadow's thicker parts crawling as the car stood).
        const float texel = 2.0f * half / static_cast<float>(kSunCell);
        const float snap = i == 0 ? texel * kSunNearStep : texel;
        const float a = std::floor(dot(centre, side) / snap) * snap;
        const float b = std::floor(dot(centre, up) / snap) * snap;
        const float c = dot(centre, sun);
        const Vec3 place = {side[0] * a + up[0] * b + sun[0] * c, side[1] * a + up[1] * b + sun[1] * c,
                            side[2] * a + up[2] * b + sun[2] * c};
        SunCascade& s = out[static_cast<size_t>(i)];
        s.frame = {side[0], up[0], sun[0], place[0], side[1], up[1], sun[1], place[1],
                   side[2], up[2], sun[2], place[2]};
        s.half = half;
        from = to;
    }
    return out;
}

std::array<float, 2> sunCell(int cascade) {
    // Cascade 0 the top left quarter, 1 the top right, 2 the bottom left, 3 the bottom right; half a texel in:
    // Direct3D 9 draws a pixel at its corner's place, a texture is read at its texels' middles.
    constexpr float kHalfTexel = 0.5f / static_cast<float>(kSunAtlas);
    return {(cascade % 2 == 1 ? 0.75f : 0.25f) + kHalfTexel, (cascade >= 2 ? 0.75f : 0.25f) + kHalfTexel};
}

std::array<float, 4 * kSunConstants> sunConstants(const std::array<SunCascade, kSunCascades>& cascades,
                                                  const Vec4& eye) {
    std::array<float, 4 * kSunConstants> c{};
    for (size_t i = 0; i < cascades.size(); ++i) {
        const SunCascade& s = cascades[i];
        const Vec3 side = column(s.frame, 0), up = column(s.frame, 1), sun = column(s.frame, 2);
        const Vec3 place = column(s.frame, 3);
        float* rows = c.data() + 12 * i;
        for (int k = 0; k < 3; ++k) {
            rows[k] = side[static_cast<size_t>(k)] / s.half;
            rows[4 + k] = up[static_cast<size_t>(k)] / s.half;
            rows[8 + k] = sun[static_cast<size_t>(k)] / kSunDepthScale;
        }
        rows[3] = -dot(place, side) / s.half;
        rows[7] = -dot(place, up) / s.half;
        // The depth from the sun's camera, kSunBackOff behind the middle.
        rows[11] = (kSunBackOff - dot(place, sun)) / kSunDepthScale;
    }
    float* more = c.data() + 12 * kSunCascades;
    more[0] = 1.0f / static_cast<float>(kSunAtlas);
    more[1] = kSunBias / kSunDepthScale;
    more[2] = static_cast<float>(kSunAtlas);
    more[3] = 0.5f / static_cast<float>(kSunAtlas);
    for (int i = 0; i < kSunCascades; ++i) {
        const std::array<float, 2> cell = sunCell(i);
        more[4 + 2 * i] = cell[0];
        more[5 + 2 * i] = cell[1];
    }
    more[12] = eye[0];
    more[13] = eye[1];
    more[14] = eye[2];
    more[15] = kSunNormalOffset;
    // The world shaders' bilinear tests: the place in texels less a half in one instruction (one constant register).
    more[16] = static_cast<float>(kSunAtlas);
    more[17] = -0.5f;
    // The first cascade's step between the soft edge's tests in texels (its texels a third of the next's: an edge
    // about as soft in both) and 1 / it.
    more[18] = kSunNearStep;
    more[19] = 1.0f / kSunNearStep;
    return c;
}

SunShadows::SunShadows(render::IRenderDevice& device) : m_device(device), m_shaders(device) {
    m_camera.setOrthographic(true);
}

SunShadows::~SunShadows() {
    unbind();
    if (m_atlas != 0) m_device.destroyTexture(m_atlas);
}

void SunShadows::unbind() {
    m_device.setSunShadowMap(0);
}

bool SunShadows::draw(TrackScene& scene, render::Camera& view, const Vec4& sun, bool turns) {
    if (!usable()) return false;
    if (m_atlas == 0) {
        m_atlas = m_device.createFloatTarget(kSunAtlas, kSunAtlas);
        log::info("sun shadows: the atlas {0} x {0} {1}", kSunAtlas, m_atlas != 0 ? "made" : "not made");
        if (m_atlas == 0) return false;
    }
    view.update();
    const Mat44& p = view.projection();
    const float tanX = p[0] != 0.0f ? 1.0f / p[0] : 1.0f;
    const float tanY = p[5] != 0.0f ? 1.0f / p[5] : 1.0f;
    const auto cascades = sunCascades(view.world(), tanX, tanY, view.nearPlane(), sun);
    ++m_draws;

    unbind();
    render::Renderer3d& r = scene.engine().renderer();
    r.endMaterial();
    scene.engine().postEffects().forgetTarget();
    const render::Renderer3d::ShadowShaders kept = r.shadowShaders;
    r.shadowShaders = m_shaders.shaders(kCullNone, false);
    m_device.setRenderTarget(m_atlas, 0);
    for (int i = 0; i < kSunCascades; ++i) {
        if (turns && i >= kSunCascades - 2 && static_cast<int>(m_draws % 2) != i - (kSunCascades - 2)) continue;
        const SunCascade& s = cascades[static_cast<size_t>(i)];
        m_drawn[static_cast<size_t>(i)] = s;
        const std::array<float, 2> cell = sunCell(i);
        render::Viewport cellView;
        cellView.x = static_cast<uint32_t>((cell[0] - 0.25f) * kSunAtlas);
        cellView.y = static_cast<uint32_t>((cell[1] - 0.25f) * kSunAtlas);
        cellView.width = kSunCell;
        cellView.height = kSunCell;
        m_device.setViewport(cellView);
        m_device.clear(render::ClearFlags::Target | render::ClearFlags::ZBuffer, 0xFFFFFFFFu, 1.0f, 0);
        const Mat34& f = s.frame;
        const float place[3] = {f[3] - f[2] * kSunBackOff, f[7] - f[6] * kSunBackOff, f[11] - f[10] * kSunBackOff};
        m_camera.local = {f[0], f[1], f[2], place[0], f[4], f[5], f[6], place[1], f[8], f[9], f[10], place[2]};
        m_camera.setFov(s.half, s.half, false);
        m_camera.setNear(0.0f);
        m_camera.setFar(kSunBackOff + s.half);
        m_camera.invalidate();
        const float constants[8] = {place[0], place[1], place[2], 1.0f / kSunDepthScale, f[2], f[6], f[10], 0.0f};
        r.setVertexConstants(240, constants, 2);
        scene.drawSunCasters(m_camera, view);
        if (i == 0) {
            r.endMaterial();
            m_device.dilateDepth(m_atlas, cellView, kSunDilateGap);
        }
    }
    r.shadowShaders = kept;
    r.endMaterial();
    m_device.setRenderTarget(0, 0);

    const Mat34& v = view.world();
    const auto c = sunConstants(m_drawn, {v[3], v[7], v[11], 0.0f});
    m_device.setPixelShaderConstantF(kSunConstant, c.data(), kSunConstants);
    // The trees' replaced effects (JuicedConfigExt's Trees.fx) read a copy in their shared parameters.
    render::ProgramContext& programs = r.programs();
    const float sunOn = 1.0f;
    programs.setShared("SunCascades", c.data(), sizeof c);
    programs.setShared("SunShadowsOn", &sunOn, sizeof sunOn);
    const int32_t on = 1;
    m_device.setPixelShaderConstantB(kSunBool, &on, 1);
    m_device.setSunShadowMap(m_atlas);
    return true;
}

}  // namespace oj::scene
