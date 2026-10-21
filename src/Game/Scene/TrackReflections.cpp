// The reflections of a race's world (docs/spec/15-tracks.md, "The reflections"): the cubes the cars and the world
// reflect (0x00547D50, 0x00548450) and the wet road's reflection (0x00546BF0 in the main pass, 0x00547690).

#include "Game/Scene/TrackScene.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Core/Physics/TrackCollision.h"
#include "Core/Base/X87.h"
#include "Game/Scene/PostEffects.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace oj::scene {

using render::ClearFlags;
using render::RenderState;
using render::TransformState;

namespace {

constexpr size_t kNoMark = std::numeric_limits<size_t>::max();

// 0x00547040: the faces of a cube centred at `focus` that a sphere reaches. Twelve tests of the centre's offset d
// (x, y, z) against -radius x 1.41421: the pairs of the axes each face looks between.
uint32_t cubeFaces(const Vec4& focus, const Vec4& centre, float radius) {
    const float dx = centre[0] - focus[0];
    const float dy = centre[1] - focus[1];
    const float dz = centre[2] - focus[2];
    const float k = -(radius * std::bit_cast<float>(0x3FB504D5u));
    uint32_t bits = 0;
    if (dz + dx > k) bits |= 0x001;
    if (dz - dx > k) bits |= 0x002;
    if (dy + dx > k) bits |= 0x004;
    if (dy - dx > k) bits |= 0x008;
    if (dz + dy > k) bits |= 0x010;
    if (dy - dz > k) bits |= 0x020;
    if (-dx - dz > k) bits |= 0x040;
    if (dx - dz > k) bits |= 0x080;
    if (-dx - dy > k) bits |= 0x100;
    if (dx - dy > k) bits |= 0x200;
    if (-dy - dz > k) bits |= 0x400;
    if (dz - dy > k) bits |= 0x800;
    uint32_t faces = 0;
    if ((bits & 0x813) == 0x813) faces = 0x10;
    if ((bits & 0x4E0) == 0x4E0) faces |= 0x20;
    if ((bits & 0x14A) == 0x14A) faces |= 0x02;
    if ((bits & 0x285) == 0x285) faces |= 0x01;
    if ((bits & 0x03C) == 0x03C) faces |= 0x04;
    if ((bits & 0xF00) == 0xF00) faces |= 0x08;
    return faces;
}

// 0x00546FD0: a sphere and a box meet (on each axis the sphere's span reaches the box's).
bool sphereMeetsBox(const Vec4& centre, float radius, const std::array<float, 4>& low,
                    const std::array<float, 4>& high) {
    for (size_t k = 0; k < 3; ++k) {
        if (centre[k] - radius > high[k]) return false;
        if (radius + centre[k] < low[k]) return false;
    }
    return true;
}

// The children of an inner node in the order the walks push them (the last pushed is taken first): the one the
// camera's place (x, z) is in is taken first, then the one beside it across x, then across z, then the far one.
std::array<uint32_t, 4> pushOrder(const RenderTreeNode& n, const Vec4& place) {
    const std::array<uint32_t, 4>& c = n.children;
    const bool lowX = place[0] < n.splitX;
    const bool lowZ = place[2] < n.splitZ;
    if (lowX) return lowZ ? std::array<uint32_t, 4>{c[3], c[2], c[1], c[0]}
                          : std::array<uint32_t, 4>{c[1], c[0], c[3], c[2]};
    return lowZ ? std::array<uint32_t, 4>{c[2], c[3], c[0], c[1]} : std::array<uint32_t, 4>{c[0], c[2], c[1], c[3]};
}

}  // namespace

void TrackScene::mirror(Instance& in, float twice) {
    in.block[7] = twice - in.block[7];
    in.block[1] = -in.block[1];
    in.block[5] = -in.block[5];
    in.block[9] = -in.block[9];
}

TrackScene::Region TrackScene::regionAt(float x, float y, float z) const {
    Region region;
    region.number = m_regions ? m_regions->region(x, y, z) : 0;
    region.word = region.number >> 5;
    region.bit = 1u << (region.number & 31);
    return region;
}

void TrackScene::loaded() {
    drawWorldCube(0.0f);
    // OpenJuice is in a frame of the loading screen: the back buffer taken again.
    PostEffects& post = m_engine.postEffects();
    post.forgetTarget();
    post.takeTarget(0);
}

void TrackScene::drawCarCube(render::Camera& camera, float clock) {
    drawCube(camera, m_engine.reflectionTargets().carCube, clock);
}

void TrackScene::drawWorldCube(float clock) {
    Engine::ReflectionTargets& targets = m_engine.reflectionTargets();
    targets.worldCubeDue = false;
    if (targets.worldCube.id == 0) return;
    // A camera of the world (0x005311D0) at the environment's place, its focus there, near 1 and far 10000000.
    m_lodScale = 0.9f;
    render::Camera camera;
    camera.setPosition(m_environmentPlace[0], m_environmentPlace[1], m_environmentPlace[2]);
    camera.setFocus(m_environmentPlace);
    camera.setNear(1.0f);
    camera.setFar(10000000.0f);
    render::Renderer3d& r = m_engine.renderer();
    if (m_atmosphere) render::applyAtmosphere(r, *m_atmosphere, camera);
    GraphicsConfig& settings = graphicsConfig();
    const int32_t reflections = settings.reflections;
    const float quality = r.drawQuality;
    settings.reflections = 3;
    r.drawQuality = 1.0f;
    drawCube(camera, targets.worldCube, clock);
    settings.reflections = reflections;
    r.drawQuality = quality;
}

void TrackScene::drawCube(render::Camera& camera, const render::GameTexture& cube, float clock) {
    render::Renderer3d& r = m_engine.renderer();
    const GraphicsConfig& settings = graphicsConfig();
    if (r.drawQuality < 0.8f || settings.reflections == 0) return;
    render::IRenderDevice& device = m_engine.device();
    r.drawingCube = true;
    ++m_cubeFrames;
    if (m_atmosphere) render::applyAtmosphere(r, *m_atmosphere, camera);
    const Mat34& c = camera.world();
    const Region region = regionAt(c[3], c[7], c[11]);
    r.time = m_clock.read(clock);
    r.endMaterial();
    r.dynamicEnvironment = m_testEnvironment;
    r.environment = m_testEnvironment;
    device.setRenderState(RenderState::FogEnable, 1);
    device.setRenderState(RenderState::ZEnable, 1);
    const Vec4 focus = camera.focus;
    if (cube.id != 0) {
        // 0x005C9190: the faces' cameras at the focus; c10 the first face's projection.
        for (render::Camera& f : m_faces) f.setPosition(focus[0], focus[1], focus[2]);
        r.setVertexConstants(10, m_faces[0].projection().data(), 4);
        collectCube(region, focus, std::sqrt(m_cubeRange));
        PostEffects& post = m_engine.postEffects();
        for (uint32_t face = 0; face < 6; ++face) {
            // Below the reflections 3 the faces of one parity by turns.
            if (settings.reflections != 3 && ((m_cubeFrames ^ face) & 1) != 0) continue;
            const uint32_t bit = 1u << face;
            // 0x005C9120: the stages emptied, the face set past the target taken last; cleared to black.
            for (uint32_t s = 0; s < 4; ++s) device.setTexture(s, 0);
            post.forgetTarget();
            device.setRenderTarget(cube.id, face);
            device.clear(ClearFlags::Target | ClearFlags::ZBuffer | ClearFlags::Stencil, 0, 1.0f, 0);
            render::Camera& f = m_faces[face];
            f.setNear(camera.nearPlane());
            f.setFar(camera.farPlane());
            f.update();
            // c7 the view, c14 world to clip, the projection transform (0x005117E0 turns it over); c33 the face's
            // direction (Stock problem: its w a stack value; 0 here) and c41, c42 the sprites' axes as the views
            // make them (0x005115C0); c32 is left as it was.
            r.setVertexConstants(7, f.view().data(), 3);
            r.setVertexConstants(14, f.worldToClip().data(), 4);
            const Mat44& p = f.projection();
            const float turned[16] = {p[0], p[4], p[8], p[12], p[1], p[5], p[9], p[13],
                                      p[2], p[6], p[10], p[14], p[3], p[7], p[11], p[15]};
            device.setTransform(TransformState::Projection, turned);
            const Mat34& w = f.world();
            const float fx = w[2], fy = w[6], fz = w[10];
            const float direction[4] = {fx, fy, fz, 0.0f};
            r.setVertexConstants(33, direction, 1);
            float sprite[8];
            scene::spriteAxes(fx, fy, fz, sprite);
            r.setVertexConstants(41, sprite, 2);
            // The opaque instances that reach the face, the sky dome, those drawn after the cars (the last found
            // first), then with the object constants of no object the flares' reflections (0x0054FE30).
            for (size_t i = 0; i < m_cubeEntries.size(); ++i) {
                if ((m_cubeFaces[i] & bit) != 0) drawInstance(f, *m_cubeEntries[i].mesh, *m_cubeEntries[i].instance);
            }
            if (m_sky) m_sky->draw(f);
            for (size_t i = m_deferred.size(); i-- > 0;) {
                if ((m_deferredFaces[i] & bit) != 0) drawInstance(f, *m_deferred[i].mesh, *m_deferred[i].instance);
            }
            identityObject(f);
            drawFlareReflections(f);
        }
        m_deferred.clear();
        m_cubeEntries.clear();
        m_deferredFaces.clear();
        m_cubeFaces.clear();
    }
    r.endMaterial();
    r.drawingCube = false;
}

void TrackScene::collectCube(const Region& region, const Vec4& focus, float radius) {
    const render::Renderer3d& r = m_engine.renderer();
    auto take = [&](const Entry& e) {
        const Instance& in = *e.instance;
        const float dz = focus[2] - in.centre[2];
        const float dy = focus[1] - in.centre[1];
        const float dx = focus[0] - in.centre[0];
        const float d = ((dx * dx + dy * dy) + dz * dz) * m_lodScale;
        if (!(r.lodBias * e.mesh->range[2] < d) || !(d <= r.lodBias * e.mesh->range[3])) return;
        const uint32_t faces = cubeFaces(focus, in.centre, in.radius);
        if (e.mesh->deferred) {
            m_deferred.push_back(e);
            m_deferredFaces.push_back(faces);
        } else {
            m_cubeEntries.push_back(e);
            m_cubeFaces.push_back(faces);
        }
    };
    // [0x0071AB9C] is 1 and nothing writes it: the region's list. The render tree's walk after it never runs.
    constexpr bool kRegionLists = true;
    if (kRegionLists) {
        if (region.number < m_lists.size()) {
            for (const Entry& e : m_lists[region.number]) take(e);
        }
        return;
    }
    if (m_tree.empty()) return;
    std::vector<uint32_t> stack{0};
    while (!stack.empty()) {
        const uint32_t at = stack.back();
        stack.pop_back();
        const RenderTreeNode& n = m_tree[at];
        if ((n.regions[region.word] & region.bit) == 0) continue;
        if (!sphereMeetsBox(focus, radius, n.low, n.high)) continue;
        if (n.leaf) {
            for (const Entry& e : m_treeEntries[at]) {
                if ((e.instance->regions[region.word] & region.bit) != 0) take(e);
            }
            continue;
        }
        for (uint32_t child : pushOrder(n, m_cameraPlace)) stack.push_back(child);
    }
}

void TrackScene::drawReflectedTree(render::Camera& camera, const Region& region, float height) {
    if (m_tree.empty()) return;
    const render::Renderer3d& r = m_engine.renderer();
    const float twice = height + height;
    // The walk's stack; `inside` the depth from which the nodes lie wholly in the view (none tested below it).
    std::vector<uint32_t> stack{0};
    size_t inside = kNoMark;
    while (!stack.empty()) {
        const uint32_t at = stack.back();
        stack.pop_back();
        const size_t depth = stack.size();
        if (inside > depth) inside = kNoMark;
        const RenderTreeNode& n = m_tree[at];
        if ((n.regions[region.word] & region.bit) == 0) continue;
        if (inside == kNoMark) {
            // The box upside down about the height.
            const Vec4 low{n.low[0], twice - n.high[1], n.low[2], 0.0f};
            const Vec4 high{n.high[0], twice - n.low[1], n.high[2], 0.0f};
            const int seen = camera.boxVisible(low, high);
            if (seen == 0) continue;
            if (seen == 1) inside = depth;
        }
        if (!n.leaf) {
            for (uint32_t child : pushOrder(n, m_cameraPlace)) stack.push_back(child);
            continue;
        }
        for (const Entry& e : m_treeEntries[at]) {
            Instance& in = *e.instance;
            if ((in.regions[region.word] & region.bit) == 0) continue;
            const float dx = m_cameraPlace[0] - in.centre[0];
            const float dy = m_cameraPlace[1] - (twice - in.centre[1]);
            const float dz = m_cameraPlace[2] - in.centre[2];
            const float d = ((dz * dz + dy * dy) + dx * dx) * m_lodScale;
            if (!(r.lodBias * e.mesh->range[4] < d) || !(d <= r.lodBias * e.mesh->range[5])) continue;
            mirror(in, twice);
            if (inside <= depth || camera.sphereVisible(in.matrix(), e.mesh->centre, in.radius)) {
                if (e.mesh->deferred) {
                    m_deferred.push_back(e);
                } else {
                    drawInstance(camera, *e.mesh, in);
                }
            }
            mirror(in, twice);
        }
    }
}

void TrackScene::drawFlareReflections(render::Camera& camera) {
    if (m_lens.empty() || m_lens.batchCount() == 0) return;
    m_engine.device().setRenderState(RenderState::ZEnable, 1);
    const Mat34& c = camera.world();
    for (Flare& f : m_lens.flares()) {
        // 0x00542100: lit and with the reflection's texture; the sun first takes its place from this camera.
        if ((f.on != nullptr && !*f.on) || f.reflectionBatch == SIZE_MAX) continue;
        if (f.def->screen != 0.0f) m_lens.placeSun(camera, f);
        m_lens.flareWorld(f);
        const float px = f.world[3], py = f.world[7], pz = f.world[11];
        const float dx = c[3] - px, dy = c[7] - py, dz = c[11] - pz;
        const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
        if (distance > 5000.0f && f.def->screen == 0.0f) continue;
        const float inverse = 1.0f / distance;
        const float ux = dx * inverse, uy = dy * inverse, uz = dz * inverse;
        float size = f.def->reflectionSize * 0.5f;
        if (f.def->screen != 0.0f) size = size * f.def->screen * std::bit_cast<float>(0x37A7C5ACu);
        // A square facing the camera, its corners along up x the way to the camera (made unit, 0x00407C00) and the
        // way to the camera x that: v0 = p - a s, v1 = p - b s, v2 = p + b s, v3 = p + a s.
        float ax = uz - uy * 0.0f, ay = ux * 0.0f - uz * 0.0f, az = uy * 0.0f - ux;
        const float k = 1.0f / (std::sqrt((ax * ax + ay * ay) + az * az) + std::bit_cast<float>(0x34000000u));
        ax = k * ax;
        ay = k * ay;
        az = k * az;
        const float bx = uy * az - uz * ay, by = uz * ax - ux * az, bz = ux * ay - uy * ax;
        FlareBatch& b = m_lens.batchAt(f.reflectionBatch);
        if (b.data.size() / 8 + 4 >= 0x800) continue;
        float white = 0.0f;
        const uint32_t colour = 0xFFFFFFFFu;
        std::memcpy(&white, &colour, 4);
        const float bsx = bx * size, bsy = by * size, bsz = bz * size;
        b.data.insert(b.data.end(), {px - ax * size, py - ay * size, pz - az * size, 0.0f, 0.0f, 0.0f, 0.0f, white});
        b.data.insert(b.data.end(), {px - bsx, py - bsy, pz - bsz, 1.0f, 0.0f, 0.0f, 0.0f, white});
        b.data.insert(b.data.end(), {bsx + px, bsy + py, pz + bsz, 0.0f, 1.0f, 0.0f, 0.0f, white});
        b.data.insert(b.data.end(), {ax * size + px, ay * size + py, az * size + pz, 1.0f, 1.0f, 0.0f, 0.0f, white});
    }
    m_lens.drawBatches();
}

void TrackScene::drawFlareRoadReflections(render::Camera& camera) {
    if (m_lens.empty() || m_lens.batchCount() == 0) return;
    m_engine.device().setRenderState(RenderState::ZEnable, 1);
    const Mat34& c = camera.world();
    for (Flare& f : m_lens.flares()) {
        // 0x00542530: lit and with the y reflection's texture.
        if ((f.on != nullptr && !*f.on) || f.roadBatch == SIZE_MAX) continue;
        m_lens.flareWorld(f);
        const float px = f.world[3], pz = f.world[11];
        // The road's height under the flare, found again when the flare moved across (0x0053F2A0: the highest face,
        // 0 without one).
        if (px != f.groundX || pz != f.groundZ) {
            float lowest = 0.0f, highest = -3.4028235e38f;
            if (m_collision) groundSpan(*m_collision, px, pz, lowest, highest);
            f.ground = highest == -3.4028235e38f ? 0.0f : highest;
            f.groundX = px;
            f.groundZ = pz;
        }
        // As far under the road as the flare is from it.
        float above = f.world[7] - f.ground;
        if (above < 0.0f) above = -above;
        const float y = f.ground - above;
        const float dx = c[3] - px, dy = c[7] - y, dz = c[11] - pz;
        const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
        if (distance > 20000.0f) continue;
        const float inverse = 1.0f / distance;
        const float ux = dx * inverse, uy = dy * inverse, uz = dz * inverse;
        // The angle off the flare's y (upside down: x and z negated, made unit) toward the camera between the
        // fall-off angles; the alphas of the top and the bottom by what is left.
        float ax = -f.world[1], ay = f.world[5], az = -f.world[9];
        const float k = 1.0f / (std::sqrt((ax * ax + ay * ay) + az * az) + std::bit_cast<float>(0x34000000u));
        ax = k * ax;
        ay = k * ay;
        az = k * az;
        float t = (((az * uz + ay * uy) + ax * ux) - f.def->cosMin) / (f.def->cosMax - f.def->cosMin);
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
        const float left = 1.0f - t;
        const uint32_t top = (static_cast<uint32_t>(x87::chop(left * f.def->roadAlphaTop)) << 24) | 0xFFFFFFu;
        const uint32_t bottom = (static_cast<uint32_t>(x87::chop(left * f.def->roadAlphaBottom)) << 24) | 0xFFFFFFu;
        FlareBatch& b = m_lens.batchAt(f.roadBatch);
        if (b.data.size() / 8 + 4 >= 0x800) continue;
        float topBits = 0.0f, bottomBits = 0.0f;
        std::memcpy(&topBits, &top, 4);
        std::memcpy(&bottomBits, &bottom, 4);
        const float w = f.def->roadWidth, h = f.def->roadHeight;
        b.data.insert(b.data.end(), {px, y, pz, 0.0f, 1.0f, w * -0.5f, h * -0.5f, bottomBits});
        b.data.insert(b.data.end(), {px, y, pz, 1.0f, 1.0f, w * 0.5f, h * -0.5f, bottomBits});
        b.data.insert(b.data.end(), {px, y, pz, 0.0f, 0.0f, w * -0.5f, h * 0.5f, topBits});
        b.data.insert(b.data.end(), {px, y, pz, 1.0f, 0.0f, w * 0.5f, h * 0.5f, topBits});
    }
    m_lens.drawBatches();
}

}  // namespace oj::scene
