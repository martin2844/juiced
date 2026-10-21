#include "Render/Sky.h"

#include "Core/Base/X87.h"

#include <bit>
#include <cmath>

namespace oj::render {

namespace {

const float kPi = std::bit_cast<float>(0x40490FDBu);
const float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
const float kOverPi = std::bit_cast<float>(0x3EA2F983u);   // 0x006FFD68
const float kHalfPi = std::bit_cast<float>(0x3FC90FDBu);   // 0x006FFB0C
constexpr float kOver255 = 1.0f / 255.0f;                  // 0x006FF918
constexpr float kRadius = 5000.0f;                         // 0x006FFCA8
constexpr float kRingDrop = 100.0f;                        // 0x006FF98C

// fistp in the rounding mode the game runs with (to nearest), the low byte.
uint32_t roundByte(float v) {
    return static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(v))) & 0xFF;
}

// 0x00407910
float wrapAngle(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

float blendValue(const AtmosphereSet& s, float Atmosphere::*field) {
    if (s.next == nullptr) return s.current->*field;
    return (1.0f - s.blend) * (s.current->*field) + s.blend * (s.next->*field);
}

uint32_t blendColour(const AtmosphereSet& s, uint32_t Atmosphere::*field) {
    if (s.next == nullptr) return s.current->*field;
    return mixColour(s.next->*field, s.current->*field, s.blend);
}

struct Rgb {
    float r, g, b;
};

Rgb rgb(uint32_t argb) {
    return {static_cast<float>((argb >> 16) & 0xFF), static_cast<float>((argb >> 8) & 0xFF),
            static_cast<float>(argb & 0xFF)};
}

// a × wa + b × wb for each channel.
Rgb sum(const Rgb& a, float wa, const Rgb& b, float wb) {
    return {a.r * wa + b.r * wb, a.g * wa + b.g * wb, a.b * wa + b.b * wb};
}

}  // namespace

uint32_t mixColour(uint32_t a, uint32_t b, float t) {
    const float u = 1.0f - t;
    uint32_t out = 0;
    for (int shift : {16, 8, 0, 24}) {
        const float v = static_cast<float>((a >> shift) & 0xFF) * t + static_cast<float>((b >> shift) & 0xFF) * u;
        out |= (static_cast<uint32_t>(x87::chop(v)) & 0xFF) << shift;
    }
    return out;
}

Vec4 atmosphereLightDirection(const AtmosphereSet& s) {
    const float* c = s.current->lightDirection[0];
    if (s.next == nullptr) return {c[0], c[1], c[2], c[3]};
    const float* n = s.next->lightDirection[0];
    const float u = 1.0f - s.blend;
    return {c[0] * u + n[0] * s.blend, c[1] * u + n[1] * s.blend, c[2] * u + n[2] * s.blend, c[3]};
}

void applyAtmosphere(Renderer3d& r, const AtmosphereSet& s, Camera& camera) {
    const Atmosphere& a = *s.current;
    const Mat34& w = camera.world();
    // The heading of the camera's Z axis against the sky's front: 0 facing it, 1 facing away.
    const float heading = static_cast<float>(std::atan2(static_cast<double>(w[8]), static_cast<double>(w[10])));
    const float f = std::fabs(wrapAngle(heading + a.skyAngle) * kOverPi);
    const uint32_t back = blendColour(s, &Atmosphere::fogBack);
    const uint32_t front = blendColour(s, &Atmosphere::fogFront);
    const uint32_t fog = mixColour(front, back, f);

    auto vec = [](const float (&v)[4]) { return Vec4{v[0], v[1], v[2], v[3]}; };
    r.setAmbient(vec(a.ambient));
    r.setLight1Colour(vec(a.lightColour[0]));
    r.setLight2Colour(vec(a.lightColour[1]));
    r.light3Colour = vec(a.lightColour[2]);
    r.writeLightColours();
    r.setLightDirection(vec(a.lightDirection[0]));
    r.setLight2Direction(vec(a.lightDirection[1]));
    r.setLight3Direction(vec(a.lightDirection[2]));
    r.setFogColour(fog);
    r.fogMin = blendValue(s, &Atmosphere::fogMin) * kOver255;
    r.applyFogDensity();
    r.fogMax = blendValue(s, &Atmosphere::fogMax) * kOver255;
    r.applyFogDensity();
    r.fogNear = blendValue(s, &Atmosphere::fogNear);
    r.applyFogDensity();
    r.fogFar = blendValue(s, &Atmosphere::fogFar);
    r.applyFogDensity();
    r.zenith = a.zenith;
    r.lightBlur = a.lightBlur;
    r.nightRoadShininess = a.nightRoadShininess;
    r.roadWetness = a.roadWetness;
    // +0x1C0: 1 for the atmosphere the set's loading put in use (0x00533950); no file key sets it (stock-problems.md).
    r.drawQuality = a.drawQuality;
    r.night = a.night != 0;
}

SkyDome::Colours skyColours(const AtmosphereSet* s) {
    if (s == nullptr || s->current == nullptr) {
        return {0xFFFFF7FF, 0xFFDAE3E9, 0xFFFFF7FF, 0xFFDAE3E9, 0xFF377EBE, 0.0f, 0.0f, 0.25f, -50.0f};
    }
    return {blendColour(*s, &Atmosphere::horizonFront),
            blendColour(*s, &Atmosphere::horizonBack),
            blendColour(*s, &Atmosphere::skyFront),
            blendColour(*s, &Atmosphere::skyBack),
            blendColour(*s, &Atmosphere::zenith),
            blendValue(*s, &Atmosphere::skyAngle),
            blendValue(*s, &Atmosphere::skyHeightFactor),
            s->current->skyLuminosity,
            s->current->skyHeightManual};
}

SkyDome::SkyDome(Renderer3d& renderer, std::unique_ptr<Material> material, const Colours& c)
    : m_r(renderer), m_material(std::move(material)), m_luminosity(c.luminosity), m_heightManual(c.heightManual) {
    // 0x005530B0: the bands, each ring's height curve e = 1 - (ring's height)^factor.
    const Rgb horizonFront = rgb(c.horizonFront), horizonBack = rgb(c.horizonBack), zenith = rgb(c.zenith);
    Rgb skyFront = rgb(c.skyFront), skyBack = rgb(c.skyBack);
    // A sky colour of 0 is halfway between the zenith and the horizon (0x00553BE0, alpha 128).
    if (c.skyFront == 0) skyFront = sum(zenith, 0.5f, horizonFront, 0.5f);
    if (c.skyBack == 0) skyBack = sum(zenith, 0.5f, horizonBack, 0.5f);

    struct Ring {
        float y, radius, e;
    };
    auto ring = [&c](uint32_t i) {
        const float a = -(kHalfPi * (static_cast<float>(i) * 0.125f));
        const double ca = std::cos(static_cast<double>(a));
        const double sa = std::sin(static_cast<double>(a));
        Ring r;
        r.y = static_cast<float>(ca * 0.0 - sa * kRadius);
        r.radius = static_cast<float>(sa * 0.0 + ca * kRadius);
        const float h = 1.0f - static_cast<float>(i) * 0.125f;
        r.e = h == 0.0f ? 1.0f
                        : static_cast<float>(1.0 - std::pow(static_cast<double>(h),
                                                            static_cast<double>(c.heightFactor)));
        return r;
    };
    // Up to e 0.5 from the horizon to the sky colour, then to the zenith.
    auto colourAt = [&zenith](float e, const Rgb& horizon, const Rgb& sky) {
        if (0.5f <= e) {
            const float w = (e - 0.5f) + (e - 0.5f);
            return sum(zenith, w, sky, 1.0f - w);
        }
        const float w = e + e;
        return sum(sky, w, horizon, 1.0f - w);
    };
    auto vertex = [](float x, float y, float z, const Rgb& colour, uint32_t alpha) {
        return Vertex{x, y, z,
                      alpha << 24 | roundByte(colour.r) << 16 | roundByte(colour.g) << 8 | roundByte(colour.b)};
    };
    const uint32_t alpha = roundByte(m_luminosity * 255.0f);
    for (uint32_t i = 0; i < 8; ++i) {
        const Ring low = ring(i);
        const Ring high = ring(i + 1);
        const float lowY = low.y - kRingDrop;
        const float highY = high.y - kRingDrop;
        Vertex last{};
        for (uint32_t j = 0; j <= 32; ++j) {
            const float t = static_cast<float>(j) * 0.03125f;
            float f = t + t;
            if (t > 0.5f) f = -(1.0f - (f - 1.0f));
            f = std::fabs(f);
            const float g = 1.0f - f;
            const Rgb horizon = sum(horizonFront, g, horizonBack, f);
            const Rgb sky = sum(skyFront, g, skyBack, f);
            const Rgb lowColour = colourAt(low.e, horizon, sky);
            const Rgb highColour = colourAt(high.e, horizon, sky);
            const float angle = t * kTwoPi + c.angle + kPi;
            const double sn = std::sin(static_cast<double>(angle));
            const float cs = static_cast<float>(std::cos(static_cast<double>(angle)));
            const float lowX = static_cast<float>(static_cast<double>(low.radius) * sn);
            const float lowZ = cs * low.radius;
            const float highX = static_cast<float>(static_cast<double>(high.radius) * sn);
            const float highZ = cs * high.radius;
            m_vertices.push_back(vertex(lowX, lowY, lowZ, lowColour, alpha));
            last = vertex(highX, highY, highZ, highColour, alpha);
            m_vertices.push_back(last);
        }
        // Two more of the band's last vertex join it to the next band.
        m_vertices.push_back(last);
        m_vertices.push_back(last);
    }

    // Position (FLOAT3) and colour (D3DCOLOR).
    VertexElement elements[3]{};
    elements[0] = {0, 0, 2, 0, 0, 0};
    elements[1] = {0, 12, 4, 0, 10, 0};
    elements[2] = {0xFF, 0, 17, 0, 0, 0};
    IRenderDevice& device = m_r.device();
    m_declaration = device.createDeclaration(elements);
    m_buffer = device.createVertexBuffer(m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
}

SkyDome::~SkyDome() {
    if (m_buffer != 0) m_r.device().destroyBuffer(m_buffer);
}

void SkyDome::draw(Camera& camera) {
    if (m_vertices.size() < 3 || m_buffer == 0) return;
    IRenderDevice& device = m_r.device();
    if (m_declaration != m_r.lastDeclaration) {
        device.setDeclaration(m_declaration);
        m_r.lastDeclaration = m_declaration;
    }
    if (m_material) m_r.beginMaterialPass(*m_material, 0);
    const Mat34& w = camera.world();
    // The dome follows the camera, lowered by its height (+0x30).
    const Mat34 model{1.0f, 0.0f, 0.0f, w[3], 0.0f, 1.0f, 0.0f, w[7] - m_heightManual, 0.0f, 0.0f, 1.0f, w[11]};
    const Mat44 modelToClip = multiply(camera.worldToClip(), model);
    m_r.setVertexConstants(0, model.data(), 3);
    m_r.setVertexConstants(4, model.data(), 3);
    m_r.setVertexConstants(18, modelToClip.data(), 4);
    device.setRenderState(RenderState::FogEnable, 0);
    device.setStream(m_buffer, sizeof(Vertex));
    device.draw(Primitive::TriangleStrip, 0, static_cast<uint32_t>(m_vertices.size()) - 2);
    device.setRenderState(RenderState::FogEnable, 1);
}

void SkyDome::drawReflected(Camera& camera) {
    if (m_vertices.size() < 3 || m_buffer == 0) return;
    IRenderDevice& device = m_r.device();
    if (m_declaration != m_r.lastDeclaration) {
        device.setDeclaration(m_declaration);
        m_r.lastDeclaration = m_declaration;
    }
    if (m_material) m_r.beginMaterialPass(*m_material, 0);
    const Mat34& w = camera.world();
    const Mat34 model{1.0f, 0.0f, 0.0f, w[3], 0.0f, -1.0f, 0.0f, w[7] + 50.0f, 0.0f, 0.0f, 1.0f, w[11]};
    const Mat44 modelToClip = multiplyAffine(camera.worldToClip(), model);
    m_r.setVertexConstants(0, model.data(), 3);
    m_r.setVertexConstants(4, model.data(), 3);
    m_r.setVertexConstants(18, modelToClip.data(), 4);
    device.setRenderState(RenderState::FogEnable, 0);
    device.setStream(m_buffer, sizeof(Vertex));
    device.draw(Primitive::TriangleStrip, 0, static_cast<uint32_t>(m_vertices.size()) - 2);
    device.setRenderState(RenderState::FogEnable, 1);
}

}  // namespace oj::render
