#include "Game/Scene/LensFlares.h"

#include "Game/Scene/Scene3d.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace oj::scene {

using render::ColourWrite;
using render::Primitive;
using render::RenderState;

namespace {


// Turned about its own x (0x00534A10 by -0.0872665).
void tiltFlare(Mat34& world) {
    const double a = static_cast<double>(std::bit_cast<float>(0xBDB2B8C3u));
    const double c = std::cos(a), s = std::sin(a);
    for (size_t row = 0; row < 3; ++row) {
        const float c1 = world[row * 4 + 1], c2 = world[row * 4 + 2];
        world[row * 4 + 1] = static_cast<float>(c * c1 - s * c2);
        world[row * 4 + 2] = static_cast<float>(s * c1 + c * c2);
    }
}

}  // namespace

LensFlares::LensFlares(Engine& engine) : m_engine(engine) {}

size_t LensFlares::batch(const std::string& texture) {
    std::vector<FlareBatch>& batches = m_engine.flareDevice().batches;
    size_t b = 0;
    while (b < batches.size() && batches[b].texture != texture) ++b;
    if (b == batches.size()) {
        render::IRenderDevice& device = m_engine.device();
        FlareBatch made;
        made.texture = texture;
        made.material = m_engine.newMaterial(17, nullptr);
        if (made.material) made.material->setTexture(0, m_engine.texture(texture));
        made.vertices = device.createVertexBuffer(std::vector<uint8_t>(0x800 * 0x20).data(), 0x800 * 0x20);
        batches.push_back(std::move(made));
    }
    return b;
}

FlareBatch& LensFlares::batchAt(size_t b) {
    return m_engine.flareDevice().batches[b];
}

size_t LensFlares::batchCount() const {
    return m_engine.flareDevice().batches.size();
}

bool LensFlares::queries() const {
    return m_engine.flareDevice().queries;
}

void LensFlares::makeReflectionBatches(Flare& f) {
    auto batchOf = [&](const std::string& texture) {
        const bool found = !texture.empty() && m_engine.texture(texture) != nullptr;
        return found ? batch(texture) : SIZE_MAX;
    };
    f.reflectionBatch = batchOf(f.def->reflection);
    f.roadBatch = batchOf(f.def->yReflection);
}

void LensFlares::makeDevice() {
    FlareDevice& d = m_engine.flareDevice();
    if (d.made) return;
    d.made = true;
    render::IRenderDevice& device = m_engine.device();
    const render::VertexElement flareElements[] = {
        {0, 0, 2, 0, 0, 0},       // position, FLOAT3
        {0, 12, 1, 0, 5, 0},      // TEXCOORD0, FLOAT2
        {0, 20, 1, 0, 5, 1},      // TEXCOORD1, FLOAT2: the corner
        {0, 28, 4, 0, 10, 0},     // COLOR, D3DCOLOR
        {0xFF, 0, 17, 0, 0, 0},
    };
    d.declaration = device.createDeclaration(flareElements);
    const render::VertexElement pointElements[] = {{0, 0, 2, 0, 0, 0}, {0xFF, 0, 17, 0, 0, 0}};
    d.pointDeclaration = device.createDeclaration(pointElements);
    // 0x0051DC10: each quad 0, 1, 2, 3, 2, 1.
    std::vector<uint16_t> indices;
    for (uint16_t q = 0; q < 0x200; ++q) {
        const auto v = static_cast<uint16_t>(q * 4);
        for (uint16_t k : {v, static_cast<uint16_t>(v + 1), static_cast<uint16_t>(v + 2), static_cast<uint16_t>(v + 3),
                           static_cast<uint16_t>(v + 2), static_cast<uint16_t>(v + 1)}) {
            indices.push_back(k);
        }
    }
    d.indices = device.createIndexBuffer(indices.data(), static_cast<uint32_t>(indices.size()));
    const float point[4] = {};
    d.point = device.createVertexBuffer(point, sizeof(point));
    d.visibility = m_engine.newMaterial(2, nullptr);
    d.queries = device.occlusionQueries();
    if (d.queries) {
        for (render::QueryId& q : d.ring) q = device.createOcclusionQuery();
    }
}

void LensFlares::test(render::Camera& camera, Flare& f) {
    if (f.on != nullptr && !*f.on) {
        f.visibility = 1.0f;
        return;
    }
    FlareDevice& d = m_engine.flareDevice();
    render::IRenderDevice& device = m_engine.device();
    const render::Renderer3d& r = m_engine.renderer();
    const float px = f.world[3], py = f.world[7], pz = f.world[11];
    // A query waited for: its pixels over 1024 (32 x 32) once it is ready (the first ten polls without a flush).
    if (f.query != -1) {
        const auto q = static_cast<size_t>(f.query);
        bool flush = true;
        if (d.polls[q] != 0) {
            --d.polls[q];
            flush = false;
        }
        uint32_t pixels = 0;
        const int result = device.queryResult(d.ring[q], flush, &pixels);
        if (result != 1) {
            if (result != 0) pixels = 0;
            f.query = -1;
            float v = static_cast<float>(pixels) * 0.0009765625f;
            if (v < 0.0f) v = 0.0f;
            else if (v > 1.0f) v = 1.0f;
            f.visibility = v;
        }
    }
    if (f.query != -1) return;
    // Within 30000 x the renderer's +0x188 of the camera and in front of it, a new test: a point at the flare, moved
    // along its y by -offset, drawn 32 pixels wide.
    const Mat34& c = camera.world();
    const float dx = c[3] - px, dy = c[7] - py, dz = c[11] - pz;
    const float reach = r.drawQuality * 30000.0f;
    if (f.def->screen == 0.0f &&
        (!((dz * dz + dy * dy) + dx * dx <= reach * reach) || (dz * c[10] + dy * c[6]) + dx * c[2] > 0.0f)) {
        f.visibility = 0.0f;
        return;
    }
    const auto q = static_cast<size_t>(d.next);
    device.issueQuery(d.ring[q], true);
    d.polls[q] = 10;
    const float point[4] = {px - f.world[1] * f.def->offset, py - f.world[5] * f.def->offset,
                            pz - f.world[9] * f.def->offset, 0.0f};
    device.updateVertexBuffer(d.point, point, sizeof(point));
    device.setStream(d.point, 16);
    device.draw(Primitive::PointList, 0, 1);
    device.issueQuery(d.ring[q], false);
    f.query = static_cast<int32_t>(q);
    d.next = (d.next + 1) & 0xFF;
}

void LensFlares::placeSun(render::Camera& camera, Flare& f) {
    // From the camera, the flare's distance toward the sun: the light's direction (the renderer's +0x40) reversed,
    // or with a SunAngle (0, -sin a, cos a) turned about y by atan2 of the light's x and z.
    const render::Renderer3d& r = m_engine.renderer();
    const Mat34& c = camera.world();
    Vec4 toward = r.lightDirection;
    if (f.def->sunAngle != 0.0f) {
        const double a = static_cast<double>(f.def->sunAngle);
        const auto s = static_cast<float>(std::sin(a));
        const float y = -s;
        const float z = static_cast<float>(std::cos(a));
        const auto yaw = static_cast<float>(
            std::atan2(static_cast<double>(r.lightDirection[0]), static_cast<double>(r.lightDirection[2])));
        // The turn about y takes the sine and the cosine unrounded (0x00407D30).
        const double sy = std::sin(static_cast<double>(yaw));
        const double cy = std::cos(static_cast<double>(yaw));
        const auto x = static_cast<float>(sy * static_cast<double>(z));
        toward = {x, y, static_cast<float>(cy * static_cast<double>(z)), 0.0f};
    }
    const float far = -f.def->screen;
    f.world[3] = c[3] + toward[0] * far;
    f.world[7] = c[7] + toward[1] * far;
    f.world[11] = c[11] + toward[2] * far;
}

void LensFlares::flareWorld(Flare& f) {
    if (f.node == nullptr) return;
    f.world = f.node->world();
    if (f.def->tilted) tiltFlare(f.world);
}

void LensFlares::quad(render::Camera& camera, Flare& f) {
    const render::Renderer3d& r = m_engine.renderer();
    if (f.on != nullptr && !*f.on) return;
    // Nothing while unseen; a sun is put in its place again here.
    if (f.visibility == 0.0f) return;
    const Mat34& c = camera.world();
    if (f.def->screen != 0.0f) placeSun(camera, f);
    const float px = f.world[3], py = f.world[7], pz = f.world[11];
    const float dx = c[3] - px, dy = c[7] - py, dz = c[11] - pz;
    const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
    // The angle off the flare's -y towards the camera between the fall-off angles: 0 within the first, 1 at the second.
    auto unit = [](float x, float y, float z) {
        const float l = 1.0f / (std::sqrt((z * z + y * y) + x * x) + 1.1920929e-7f);
        return std::array<float, 3>{x * l, y * l, z * l};
    };
    const auto toCamera = unit(dx, dy, dz);
    const auto axis = unit(-f.world[1], -f.world[5], -f.world[9]);
    float t = (((axis[2] * toCamera[2] + axis[1] * toCamera[1]) + axis[0] * toCamera[0]) - f.def->cosMin) /
              (f.def->cosMax - f.def->cosMin);
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        return;
    }
    // Faded out from 20000 to 30000 x the renderer's +0x188, times the visibility; the size by the distance seen
    // through the field of view (d = sin x distance): d / (d + 2000) x LIGHTFLARE_SIZE x (1 - t) x 0.5 over d x 0.002
    // held within 0.2 .. 1.
    float fade = 1.0f;
    const float left = 1.0f - t;
    float size = left * f.def->size * 0.5f;
    if (f.def->screen == 0.0f) {
        fade = (1.0f - (distance - r.drawQuality * 20000.0f) / (r.drawQuality * 10000.0f)) * f.visibility;
        if (fade < 0.0f) fade = 0.0f;
        else if (fade > 1.0f) fade = 1.0f;
        const float seen = camera.fovSin() * distance;
        float scale = seen * 0.002f;
        if (scale < 0.2f) scale = 0.2f;
        else if (scale > 1.0f) scale = 1.0f;
        size = seen / (seen + 2000.0f) * f.def->size * left * 0.5f / scale;
    }
    if (!(fade > 0.003921569f)) return;
    if (!camera.sphereVisible({px, py, pz, f.world[11]}, size)) return;
    const auto alpha = static_cast<uint32_t>(static_cast<int32_t>(left * fade * 255.0f));
    const uint32_t colour = (alpha << 24) | 0xFFFFFFu;
    FlareBatch& b = batchAt(f.batch);
    if (b.data.size() / 8 + 4 >= 0x800) return;
    float colourBits = 0.0f;
    std::memcpy(&colourBits, &colour, 4);
    const float corners[4][4] = {{0.0f, 0.0f, -size, -size}, {1.0f, 0.0f, size, -size}, {0.0f, 1.0f, -size, size},
                                 {1.0f, 1.0f, size, size}};
    for (const auto& k : corners) {
        b.data.insert(b.data.end(), {px, py, pz, k[0], k[1], k[2], k[3], colourBits});
    }
}

void LensFlares::draw(render::Camera& camera) {
    if (m_flares.empty()) return;
    // A car's flare is where its node is now; car_flare01 turned about its own x by -0.0872665 (0x00534A10). Then
    // every sun is put in its place from this camera before the visibility tests (0x00540A70).
    for (Flare& f : m_flares) {
        flareWorld(f);
        if (f.def->screen != 0.0f) placeSun(camera, f);
    }
    FlareDevice& d = m_engine.flareDevice();
    render::Renderer3d& r = m_engine.renderer();
    render::IRenderDevice& device = m_engine.device();
    device.setRenderState(RenderState::ZEnable, 0);
    if (!d.queries) {
        for (Flare& f : m_flares) f.visibility = 1.0f;
    } else if (d.visibility) {
        // 0x00540BA0: the visibility test's material and states.
        device.setDeclaration(d.pointDeclaration);
        r.lastDeclaration = d.pointDeclaration;
        r.beginMaterialPass(*d.visibility, 0);
        device.setRenderState(RenderState::PointScaleEnable, 0);
        device.setRenderState(RenderState::PointSize, std::bit_cast<uint32_t>(32.0f));
        device.setRenderState(RenderState::ZEnable, 1);
        device.setRenderState(RenderState::ColourWriteEnable, ColourWrite::None);
        device.setRenderState(RenderState::ZWriteEnable, 0);
        device.setRenderState(RenderState::AlphaBlendEnable, 0);
        for (Flare& f : m_flares) test(camera, f);
        device.setRenderState(RenderState::ZWriteEnable, 1);
        device.setRenderState(RenderState::ColourWriteEnable, ColourWrite::All);
    }
    for (Flare& f : m_flares) quad(camera, f);
    device.setRenderState(RenderState::ZEnable, 1);
    drawBatches();
}

void LensFlares::drawBatches() {
    FlareDevice& d = m_engine.flareDevice();
    render::Renderer3d& r = m_engine.renderer();
    render::IRenderDevice& device = m_engine.device();
    for (FlareBatch& b : d.batches) {
        if (b.data.empty() || !b.material) {
            b.data.clear();
            continue;
        }
        const auto count = static_cast<uint32_t>(b.data.size() / 8);
        device.updateVertexBuffer(b.vertices, b.data.data(), static_cast<uint32_t>(b.data.size() * 4));
        device.setDeclaration(d.declaration);
        r.lastDeclaration = d.declaration;
        r.beginMaterialPass(*b.material, 0);
        device.setRenderState(RenderState::ZEnable, d.queries ? 0 : 1);
        device.setStream(b.vertices, 0x20);
        device.setIndices(d.indices);
        device.drawIndexed(Primitive::TriangleList, count, 0, count / 4 * 2);
        b.data.clear();
    }
    device.setRenderState(RenderState::ZEnable, 1);
}

}  // namespace oj::scene
