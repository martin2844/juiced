#include "Render/Renderer3d.h"

#include "Core/Base/X87.h"
#include "Render/Materials.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

namespace oj::render {

namespace {

constexpr float kOver511 = 1.0f / 511.0f;          // 0x006FF778
constexpr float kOver255 = 1.0f / 255.0f;          // 0x006FF918
constexpr float kOver255x511 = 1.0f / 130305.0f;   // 0x006FFC20
constexpr float kEpsilon = FLT_EPSILON;            // 0x006FF99C


uint32_t bits(float f) {
    uint32_t v = 0;
    std::memcpy(&v, &f, 4);
    return v;
}

}  // namespace

Renderer3d::Renderer3d(IRenderDevice& device, ProgramContext& programs) : m_device(device), m_programs(programs) {}

GameTexture makeRendererCube(IRenderDevice& device, uint32_t edge, bool halfVector) {
    constexpr uint32_t kA8R8G8B8 = 21;
    const bool wide = device.backBufferFormat() == kA8R8G8B8;
    const uint32_t texel = wide ? 4 : 2;
    std::vector<uint8_t> pixels(size_t(edge) * edge * 6 * texel);
    const auto size = static_cast<float>(edge);
    size_t at = 0;
    for (uint32_t face = 0; face < 6; ++face) {
        for (uint32_t row = 0; row < edge; ++row) {
            const float rf = static_cast<float>(row) / size;
            const float t = (rf + rf) - 1.0f;
            for (uint32_t col = 0; col < edge; ++col) {
                const float cf = static_cast<float>(col) / size;
                const float s = (cf + cf) - 1.0f;
                float v[3] = {};
                switch (face) {
                    case 0: v[0] = 1.0f; v[1] = -t; v[2] = -s; break;
                    case 1: v[0] = -1.0f; v[1] = -t; v[2] = s; break;
                    case 2: v[0] = s; v[1] = 1.0f; v[2] = t; break;
                    case 3: v[0] = s; v[1] = -1.0f; v[2] = -t; break;
                    case 4: v[0] = s; v[1] = -t; v[2] = 1.0f; break;
                    default: v[0] = -s; v[1] = -t; v[2] = -1.0f; break;
                }
                const float k = 1.0f / (std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]) + kEpsilon);
                float x = v[0] * k, y = v[1] * k, z = v[2] * k;
                uint8_t r = 0, g = 0, b = 0;
                if (halfVector) {
                    z = z - 1.0f;
                    const float h = 1.0f / (std::sqrt((y * y + x * x) + z * z) + kEpsilon);
                    // 0x005CA370: fistp, to nearest (the main thread's x87 after the device is made).
                    auto byteOf = [](float c) {
                        return static_cast<uint8_t>(static_cast<int32_t>(std::nearbyint(c * 127.0f + 127.0f)));
                    };
                    r = byteOf(x * h);
                    g = byteOf(h * y);
                    b = byteOf(h * z);
                } else {
                    const float h = 1.0f / (std::sqrt((z * z + y * y) + x * x) + kEpsilon);
                    auto byteOf = [](float c) { return static_cast<uint8_t>(static_cast<int32_t>(c * 127.0f + 127.0f)); };
                    r = byteOf(x * h);
                    g = byteOf(h * y);
                    b = byteOf(h * z);
                }
                if (wide) {
                    pixels[at++] = b;
                    pixels[at++] = g;
                    pixels[at++] = r;
                    pixels[at++] = 1;
                } else {
                    const auto p = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                    pixels[at++] = static_cast<uint8_t>(p & 0xFF);
                    pixels[at++] = static_cast<uint8_t>(p >> 8);
                }
            }
        }
    }
    TextureData data;
    data.format = wide ? PixelFormat::A8R8G8B8 : PixelFormat::R5G6B5;
    data.width = edge;
    data.height = edge;
    data.levels = 1;
    data.pixels = pixels.data();
    data.size = pixels.size();
    GameTexture texture;
    texture.id = device.createCubeTexture(data);
    texture.sampler = {2, 2, 2, 3, 3, 0.0f};
    return texture;
}

void Renderer3d::reset() {
    const float numbers[4] = {1.0f, 0.5f, 0.25f, 0.0f};
    setVertexConstants(24, numbers, 1);
    // The renderer's ambient, light colour and direction vectors leave w as a stack value of the stock code.
    setAmbient({51.0f, 51.0f, 51.0f, 0.0f});
    setLight1Colour({255.0f, 255.0f, 255.0f, 0.0f});
    setLightDirection({0.717f, -0.5f, 0.0f, 0.0f});
    // c39 takes the stack where the light colour was built, with 100 after it.
    const float power[4] = {255.0f, 255.0f, 255.0f, 100.0f};
    setVertexConstants(39, power, 1);
    setSpecular(0xFF000000, 0.0f);
    m_device.setRenderState(RenderState::FogTableMode, FogMode::Exp);
    m_device.setRenderState(RenderState::FogVertexMode, FogMode::None);
    m_device.setRenderState(RenderState::RangeFogEnable, 0);
    m_device.setRenderState(RenderState::FogDensity, 0);
    m_device.setRenderState(RenderState::CullMode, Cull::CounterClockwise);
    m_device.setRenderState(RenderState::AlphaRef, 0x80);
    m_device.setRenderState(RenderState::AlphaFunc, CompareFunc::GreaterEqual);
    m_device.setRenderState(RenderState::ZFunc, CompareFunc::LessEqual);
    m_device.setRenderState(RenderState::ZEnable, 1);
    m_device.setRenderState(RenderState::DitherEnable, 1);
    endMaterial();
    for (uint32_t stage = 0; stage < 4; ++stage) {
        m_device.setTextureStageState(stage, TextureStageState::TextureTransformFlags, TextureTransform::Disable);
    }
    const Mat44 identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    m_device.setTransform(TransformState::World, identity.data());
    m_device.setTransform(TransformState::View, identity.data());
}

void Renderer3d::setAmbient(const Vec4& colour) {
    ambient = colour;
    const float c[4] = {colour[0] * kOver511, colour[1] * kOver511, colour[2] * kOver511, 1.0f};
    setVertexConstants(25, c, 1);
    fixedFunctionLights();
}

void Renderer3d::fixedFunctionMaterial() {
    if (classes() != 0x10000) return;
    FixedMaterial m;
    for (int i = 0; i < 8; ++i) m.values[i] = 1.0f;  // diffuse, ambient
    for (int i = 8; i < 11; ++i) m.values[i] = 1.0f;  // specular r, g, b
    m.values[16] = 17.0f;
    m_device.setMaterial(m);
}

void Renderer3d::fixedFunctionLights() {
    if (classes() != 0x10000) return;
    fixedFunctionMaterial();
    const Vec4* colours[3] = {&light1Colour, &light2Colour, &light3Colour};
    const Vec4* directions[3] = {&lightDirection, &light2Direction, &light3Direction};
    for (uint32_t i = 0; i < 3; ++i) {
        FixedLight l;
        l.type = 3;  // D3DLIGHT_DIRECTIONAL
        // Stock problem: the green of the diffuse colour is the light's red again (0x00546419).
        const float red = (*colours[i])[0] * 0.00390625f;
        l.diffuse[0] = red;
        l.diffuse[1] = red;
        l.diffuse[2] = (*colours[i])[2] * 0.00390625f;
        l.specular[0] = l.specular[1] = l.specular[2] = 0.6f;
        l.ambient[0] = ambient[0] * 0.01f;
        l.ambient[1] = ambient[1] * 0.01f;
        l.ambient[2] = ambient[2] * 0.01f;
        l.direction[0] = (*directions[i])[0];
        l.direction[1] = (*directions[i])[1];
        l.direction[2] = (*directions[i])[2];
        m_device.setLight(i, l);
        m_device.lightEnable(i, true);
    }
    m_device.setRenderState(RenderState::Lighting, 1);
    // D3DRS_AMBIENT: the ambient's w, r, g, b each made a whole number (0x005CA370), a byte of it.
    const uint32_t argb = (static_cast<uint32_t>(x87::nearest(ambient[3]) & 0xFF) << 24) |
                          (static_cast<uint32_t>(x87::nearest(ambient[0]) & 0xFF) << 16) |
                          (static_cast<uint32_t>(x87::nearest(ambient[1]) & 0xFF) << 8) |
                          static_cast<uint32_t>(x87::nearest(ambient[2]) & 0xFF);
    m_device.setRenderState(RenderState::Ambient, argb);
    m_device.setRenderState(RenderState::AmbientMaterialSource, MaterialSource::Material);
    m_device.setRenderState(RenderState::LocalViewer, 0);
}

void Renderer3d::setLight1Colour(const Vec4& colour) {
    light1Colour = colour;
    const float c[4] = {lightScale * colour[0] * kOver511, lightScale * colour[1] * kOver511,
                        lightScale * colour[2] * kOver511, lightScale};
    setVertexConstants(26, c, 1);
}

void Renderer3d::setLight2Colour(const Vec4& colour) {
    light2Colour = colour;
    const float f = night ? 1.0f : lightScale;
    const float c[4] = {f * colour[0] * kOver511, f * colour[1] * kOver511, f * colour[2] * kOver511, 1.0f};
    setVertexConstants(28, c, 1);
}

void Renderer3d::writeLightColours() {
    const float f = night ? 1.0f : lightScale;
    std::array<Vec4, 3> light = {
        Vec4{light1Colour[0] * lightScale * kOver511, light1Colour[1] * lightScale * kOver511,
             light1Colour[2] * lightScale * kOver511, lightScale},
        Vec4{f * light2Colour[0] * kOver511, f * light2Colour[1] * kOver511, f * light2Colour[2] * kOver511, 1.0f},
        Vec4{light3Colour[0] * kOver511, light3Colour[1] * kOver511, light3Colour[2] * kOver511, 1.0f},
    };
    // Brightest first ([0x00725780] is set): a light moves up while its (z + y) + x is above the one before.
    auto sum = [](const Vec4& v) { return (v[2] + v[1]) + v[0]; };
    bool swapped = true;
    while (swapped) {
        swapped = false;
        if (sum(light[0]) < sum(light[1])) {
            std::swap(light[0], light[1]);
            swapped = true;
        }
        if (sum(light[1]) < sum(light[2])) {
            std::swap(light[1], light[2]);
            swapped = true;
        }
    }
    // Stock problem: the three colours go to c26, c27, c28 although the shaders read them from c26, c28, c30
    // (c27 is the first light's direction): c28 gets the third colour, c30 none.
    float block[12];
    for (size_t i = 0; i < 3; ++i) std::memcpy(block + i * 4, light[i].data(), 16);
    setVertexConstants(26, block, 3);
}

void Renderer3d::setLightDirection(const Vec4& d) {
    lightDirection = d;
    // Stock problem: w of c27, c43 and c44 is a stack value of the stock code; 0 here.
    setVertexConstants(27, d.data(), 1);
    constexpr float zero = 0.0f;  // the products with the zero constant are kept: they can give -0
    // u = d × (1, 0, 0) as the game computes it, v = normalize(u × d), w = normalize(d × v).
    const float ux = d[1] * zero - d[2] * zero;
    const float uy = d[2] - d[0] * zero;
    const float uz = d[0] * zero - d[1];
    float vx = uy * d[2] - uz * d[1];
    float vy = uz * d[0] - ux * d[2];
    float vz = ux * d[1] - uy * d[0];
    const float vInverse = 1.0f / (std::sqrt((vz * vz + vy * vy) + vx * vx) + kEpsilon);
    vx *= vInverse;
    vy *= vInverse;
    vz *= vInverse;
    float wx = vz * d[1] - vy * d[2];
    float wy = vx * d[2] - vz * d[0];
    float wz = vy * d[0] - vx * d[1];
    const float wInverse = 1.0f / (std::sqrt((wz * wz + wy * wy) + wx * wx) + kEpsilon);
    wx *= wInverse;
    wy *= wInverse;
    wz *= wInverse;
    const float axes[8] = {vx, vy, vz, 0.0f, wx, wy, wz, 0.0f};
    setVertexConstants(43, axes, 2);
    if (classes() < 0x70000) return;

    // The clouds: c45 the time, c46 .. c48 the axes and the direction scaled, c49 the static after them.
    float t = time;
    float s = 0.0f;
    m_cloud[0] = m_cloud[1] = m_cloud[2] = 0.0f;
    switch (cloudMode) {
        case 0:
            t = time * 2.25f;
            s = 3.33333325e-06f;
            break;
        case 1:
            m_cloud[0] = time;
            m_cloud[1] = 1.0f;
            s = 4.99999987e-05f;
            break;
        case 2:
            t = time * 0.0f;
            s = 4.99999987e-05f;
            break;
        default:
            break;
    }
    m_cloudGuard = true;
    const float scroll = t * 0.015f;
    const float rows[12] = {vx * s, vy * s, vz * s, scroll, wx * s, wy * s, wz * s, 0.0f,
                            d[0] * s, d[1] * s, d[2] * s, scroll};
    std::memcpy(m_cloud.data() + 4, rows, sizeof(rows));
    // The static that follows the array is the guard of its initialisation: c49.x is 1 as an integer.
    const uint32_t guard = m_cloudGuard ? 1u : 0u;
    std::memcpy(&m_cloud[16], &guard, 4);
    setVertexConstants(45, m_cloud.data(), 5);
}

void Renderer3d::setLight2Direction(const Vec4& direction) {
    light2Direction = direction;
    setVertexConstants(29, direction.data(), 1);
}

void Renderer3d::setLight3Direction(const Vec4& direction) {
    light3Direction = direction;
    setVertexConstants(31, direction.data(), 1);
}

void Renderer3d::setSpecular(uint32_t argb, float highlight) {
    const float r = static_cast<float>((argb >> 16) & 0xFF);
    const float g = static_cast<float>((argb >> 8) & 0xFF);
    const float b = static_cast<float>(argb & 0xFF);
    const float c[4] = {r * light1Colour[0] * lightScale * kOver255x511,
                        g * light1Colour[1] * lightScale * kOver255x511,
                        b * light1Colour[2] * lightScale * kOver255x511, highlight * kOver255};
    setVertexConstants(34, c, 1);
}

void Renderer3d::setLightScale(float scale) {
    lightScale = scale;
    setLight1Colour(light1Colour);
    setLight2Colour(light2Colour);
}

void Renderer3d::setRandom(const Vec4& value) {
    random = value;
    setVertexConstants(40, value.data(), 1);
    if (m_current != nullptr && (m_current->flags() & 0x1000) != 0) m_current = nullptr;
}

void Renderer3d::setFogColour(uint32_t argb) {
    m_device.setRenderState(RenderState::FogColour, argb);
    const float c[4] = {static_cast<float>((argb >> 16) & 0xFF) * kOver255,
                        static_cast<float>((argb >> 8) & 0xFF) * kOver255, static_cast<float>(argb & 0xFF) * kOver255,
                        0.0f};
    setVertexConstants(50, c, 1);
}

void Renderer3d::applyFogDensity() {
    const float density = (fogNear == FLT_MAX || fogFar == FLT_MAX) ? 0.0f : 0.5f / fogFar;
    m_device.setRenderState(RenderState::FogDensity, bits(density));
    // Stock problem: c51.yzw are the stack after the density (a return address); 0 here.
    const float c[4] = {density, 0.0f, 0.0f, 0.0f};
    setVertexConstants(51, c, 1);
}

void Renderer3d::bindTexture(const GameTexture* texture, uint32_t stage) {
    if (texture == nullptr) {
        m_device.setTexture(stage, 0);
        return;
    }
    applySampler(texture->sampler, stage);
    m_device.setTexture(stage, texture->id);
}

std::array<const SamplerParams*, 8>& samplerCache() {
    static std::array<const SamplerParams*, 8> cache{};
    return cache;
}

const SamplerParams& internSampler(const SamplerParams& p) {
    static std::deque<SamplerParams> sets;
    for (const SamplerParams& s : sets) {
        if (s.minFilter == p.minFilter && s.magFilter == p.magFilter && s.mipFilter == p.mipFilter &&
            s.addressU == p.addressU && s.addressV == p.addressV && s.lodBias == p.lodBias) {
            return s;
        }
    }
    sets.push_back(p);
    return sets.back();
}

void applySampler(IRenderDevice& device, const SamplerParams& p, uint32_t stage) {
    std::array<const SamplerParams*, 8>& cache = samplerCache();
    const SamplerParams* set = &internSampler(p);
    if (stage < cache.size() && cache[stage] != set) {
        device.setSamplerState(stage, SamplerState::MinFilter, set->minFilter);
        device.setSamplerState(stage, SamplerState::MagFilter, set->magFilter);
        device.setSamplerState(stage, SamplerState::MipFilter, set->mipFilter);
        device.setSamplerState(stage, SamplerState::AddressU, set->addressU);
        device.setSamplerState(stage, SamplerState::AddressV, set->addressV);
        device.setSamplerState(stage, SamplerState::MipMapLodBias, bits(set->lodBias));
        cache[stage] = set;
    }
}

void Renderer3d::applySampler(const SamplerParams& p, uint32_t stage) {
    render::applySampler(m_device, p, stage);
}

void Renderer3d::beginMaterialPass(Material& material, uint32_t pass) {
    if (m_current != nullptr && m_current != &material) {
        if (Program* p = m_current->program()) p->end();
    }
    // Software vertex processing by the program's technique: the D3D9 device here processes vertices in hardware.
    if (m_lastClass != material.classId() || m_lastPass != pass) material.setup(pass);
    if (m_current != &material || m_lastPass != pass) {
        material.bind(pass);
        m_lastClass = material.classId();
        m_current = &material;
    }
}

void Renderer3d::endMaterial() {
    if (m_current != nullptr) {
        if (Program* p = m_current->program()) p->end();
    }
    m_lastClass = 0x10009;
    m_current = nullptr;
}

}  // namespace oj::render
