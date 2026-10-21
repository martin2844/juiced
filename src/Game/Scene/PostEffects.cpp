#include "Game/Scene/PostEffects.h"

#include "Core/Saves/GraphicsConfig.h"
#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Game/Scene/Scene3d.h"
#include "Render/Camera.h"
#include "Render/Program.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oj::scene {

using render::BlendFactor;
using render::ClearFlags;
using render::ColourWrite;
using render::CompareFunc;
using render::Fvf;
using render::Primitive;
using render::RenderState;
using render::SamplerState;
using render::TextureAddress;
using render::TextureArg;
using render::TextureOp;
using render::TextureStageState;
using render::TextureTransform;

namespace {

// A 2D vertex of the declaration +0x1E8 (0x00513662): the place in the view's pixels, the colour, u v.
struct Vertex2d {
    float x, y;
    uint32_t colour;
    float u, v;
};

// 0x00719324: the luminance of the exposure's measure, ps_1_1 `dp3 r0, c0, t0` with c0 (0.3, 0.59, 0.11, 0.2).
constexpr uint32_t kLuminance[] = {0xFFFF0101, 0x00000051, 0xA00F0000, 0x3E99999A, 0x3F170A3D,
                                   0x3DE147AE, 0x3E4CCCCD, 0x00000042, 0xB00F0000, 0x00000008,
                                   0x800F0000, 0xA0E40000, 0xB0E40000, 0x0000FFFF};

// A float's alpha byte at the top of a colour, cut toward zero: the stock code's conversion (x87 with the chop mode,
// or cvtss2si under the game's MXCSR, set to round toward zero by 0x005A3D59).
uint32_t alphaOf(float value) {
    return static_cast<uint32_t>(x87::chop(value)) << 24;
}

// The exposure's measure ([0x00722A88] +0x10 ..): the histogram of the measure's blue bytes, the 2 % low and high
// ends (+0x410, +0x414), the mean (+0x418), the high end again (+0x41C), the exposure (+0x420, [0x00722EA8]) and
// its speed (+0x424, [0x00722EAC]); both 0 as the game starts (Stock problem: the first frame with the glow is black
// until a camera first leaves a car's node).
struct ExposureMeasure {
    std::array<float, 256> histogram{};
    float low = 0.0f;
    float high = 0.0f;
    float mean = 0.0f;
    float high2 = 0.0f;
    float exposure = 0.0f;
    float speed = 0.0f;
};
ExposureMeasure g_exposure;

// [0x00725764] .. [0x0072577C]
constexpr float kExposureStep = 0.02f;
constexpr float kExposureMin = 0.8f;
constexpr float kExposureMax = 2.0f;
constexpr float kExposureKeep = 0.5f;
constexpr float kExposureAim = 0.8f;
constexpr float kHighShare = 0.02f;
constexpr float kLowShare = 0.02f;
constexpr float kByte = 0.003921569f;  // [0x006FF918]: 1 / 255

// TexShape2D (0x10002, vtable 0x006F9594, 0x0052CE30): Material_TexShape2D (+0x1C), the texture (+0x20), additive
// (+0x24, parameter 0) and opaque (+0x28, parameter 1).
class TexShape2d : public render::Material {
public:
    TexShape2d(render::Renderer3d& r, render::Program* program) : Material(kTexShape2D, 0, r), m_program(program) {}

    // 0x00529810
    void setParam(uint32_t index, const void* value) override {
        touched();
        uint32_t v = 0;
        std::memcpy(&v, value, 4);
        if (index == 0) m_additive = v;
        if (index == 1) m_opaque = v;
    }
    // 0x0052F500
    bool setTexture(uint32_t slot, const render::GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    // 0x0052CF20: the pass, the default cull, sampler 0 clamped, the texture on stage 0; opaque no blending, else
    // the source's alpha over the target (or added to it).
    void bind(uint32_t pass) override {
        render::IRenderDevice& d = m_r.device();
        if (m_program != nullptr) m_program->beginPass(pass);
        d.setRenderState(RenderState::CullMode, m_r.defaultCull);
        d.setSamplerState(0, SamplerState::AddressU, TextureAddress::Clamp);
        d.setSamplerState(0, SamplerState::AddressV, TextureAddress::Clamp);
        m_r.bindTexture(m_texture, 0);
        if (m_opaque != 0) {
            d.setRenderState(RenderState::AlphaBlendEnable, 0);
            return;
        }
        d.setRenderState(RenderState::AlphaBlendEnable, 1);
        d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
        d.setRenderState(RenderState::DestBlend, m_additive != 0 ? BlendFactor::One : BlendFactor::InvSrcAlpha);
    }
    render::Program* program() override { return m_program; }

private:
    render::Program* m_program;
    const render::GameTexture* m_texture = nullptr;
    uint32_t m_additive = 0;
    uint32_t m_opaque = 0;
};

// MotionBlur (0x10003, vtable 0x006F94FC, 0x0052C130): Material_MotionBlur (+0x20), the texture (+0x1C) on stages 0
// .. 3 (0x0052C220); no parameters.
class MotionBlur2d : public render::Material {
public:
    MotionBlur2d(render::Renderer3d& r, render::Program* program) : Material(kMotionBlur, 0, r), m_program(program) {}

    void setParam(uint32_t, const void*) override {}
    // 0x0052A3D0
    bool setTexture(uint32_t slot, const render::GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        for (uint32_t stage = 0; stage < 4; ++stage) m_r.bindTexture(m_texture, stage);
    }
    render::Program* program() override { return m_program; }

private:
    render::Program* m_program;
    const render::GameTexture* m_texture = nullptr;
};

// 2DBlur (0x10005, vtable 0x006F92BC, 0x00529970): Material_2DBlur (+0x20), the texture (+0x1C) on stages 0 .. 3, its
// value (+0x24, parameter 0, -1 at first). The pass (0x00529AA0) by the value: 0 and more the blend factor's alpha
// value x 255 (cut toward zero) with the factor over its inverse and the offsets c52 .. c55 of 1; below -1.5 the
// source's alpha added and the offsets of 0.8; else the target's inverse alpha over its alpha and the offsets of 1.
class Blur2d : public render::Material {
public:
    Blur2d(render::Renderer3d& r, render::Program* program) : Material(kBlur2D, 0, r), m_program(program) {}

    // 0x00529E30
    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) std::memcpy(&m_value, value, 4);
    }
    bool setTexture(uint32_t slot, const render::GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        for (uint32_t stage = 0; stage < 4; ++stage) m_r.bindTexture(m_texture, stage);
        render::IRenderDevice& d = m_r.device();
        // [0x007A25C0], [0x007A2600], [0x007A2640]: made once each.
        static const float kOnes[16] = {-1, -1, 0, 0, -1, 1, 0, 0, 1, -1, 0, 0, 1, 1, 0, 0};
        static const float kNear[16] = {-0.8f, -0.8f, 0, 0, -0.8f, 0.8f, 0, 0, 0.8f, -0.8f, 0, 0, 0.8f, 0.8f, 0, 0};
        if (0.0f <= m_value) {
            const auto alpha = static_cast<uint32_t>(static_cast<int32_t>(m_value * 255.0f)) << 24;
            d.setRenderState(RenderState::BlendFactor, alpha);
            d.setRenderState(RenderState::SrcBlend, BlendFactor::Constant);
            d.setRenderState(RenderState::DestBlend, BlendFactor::InvConstant);
            m_r.programs().managerVertexConstants(52, kOnes, 4);
        } else if (m_value < -1.5f) {
            d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
            d.setRenderState(RenderState::DestBlend, BlendFactor::One);
            m_r.programs().managerVertexConstants(52, kNear, 4);
        } else {
            d.setRenderState(RenderState::SrcBlend, BlendFactor::InvDestAlpha);
            d.setRenderState(RenderState::DestBlend, BlendFactor::DestAlpha);
            m_r.programs().managerVertexConstants(52, kOnes, 4);
        }
    }
    render::Program* program() override { return m_program; }

private:
    render::Program* m_program;
    const render::GameTexture* m_texture = nullptr;
    float m_value = -1.0f;
};

const float kMinusTwo = -2.0f;  // 2DBlur's value of the post effects (0x00520F10, 0x00521D9C)

// The transformed vertex of 0x00520FC0 (FVF 0x444): x y z rhw, the colour, four u v.
struct BlurVertex {
    float x, y, z, rhw;
    uint32_t colour;
    float uv[8];
};

}  // namespace

std::unique_ptr<render::Material> makeTexShape2d(Engine& engine) {
    return std::make_unique<TexShape2d>(engine.renderer(), engine.program("Material_TexShape2D"));
}

void resetExposure() {
    g_exposure.speed = 0.0f;
    g_exposure.exposure = 1.0f;
}

PostEffects::PostEffects(Engine& engine)
    : m_engine(engine), m_device(engine.device()), m_r(engine.renderer()) {
    const GraphicsConfig& c = graphicsConfig();
    const render::VertexElement elements[] = {
        {0, 0, 1, 0, 0, 0},    // POSITION, FLOAT2
        {0, 8, 4, 0, 10, 0},   // COLOR, D3DCOLOR
        {0, 12, 1, 0, 5, 0},   // TEXCOORD0, FLOAT2
        {0xFF, 0, 17, 0, 0, 0},
    };
    m_declaration2d = m_device.createDeclaration(elements);
    // The scene's targets have the screen's size (+0x34, +0x38: the settings' screen); the window's here.
    if (c.sceneWidth != 0) {
        makeTarget(m_targets[0], m_device.width(), m_device.height());
        makeTarget(m_targets[1], m_device.width(), m_device.height());
    }
    const std::vector<uint8_t> zeros(64 * 0x34);
    m_buffer = m_device.createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
    m_buffer2 = m_device.createVertexBuffer(zeros.data(), static_cast<uint32_t>(zeros.size()));
    // 0x0051FA00: the blur targets, each taken and cleared to white with no alpha.
    makeTarget(m_blurSmall, c.blurWidth, c.blurHeight);
    take(&m_blurSmall);
    m_device.clear(ClearFlags::Target, 0x00FFFFFFu, 1.0f, 0);
    makeTarget(m_blurWide, c.wideBlurWidth, c.wideBlurHeight);
    take(&m_blurWide);
    m_device.clear(ClearFlags::Target, 0x00FFFFFFu, 1.0f, 0);
    take(nullptr);
    m_texShape = std::make_unique<TexShape2d>(m_r, engine.program("Material_TexShape2D"));
    m_motionBlur = std::make_unique<MotionBlur2d>(m_r, engine.program("Material_MotionBlur"));
    m_blur = std::make_unique<Blur2d>(m_r, engine.program("Material_2DBlur"));
    // 0x00512210: the measure's target 128 x 64 (A8R8G8B8, lockable) and its pixel shader.
    m_measure = m_device.createRenderTarget(128, 64, false);
    m_luminance = m_device.createPixelShader(kLuminance);
    log::info("post effects: scene targets {}x{}, measure {}; motion blur {}, glow {}, depth of field {}, shaders {}",
              m_targets[0].width, m_targets[0].height, m_measure != 0, c.motionBlur, c.glow, c.depthOfField,
              shaders());
}

PostEffects::~PostEffects() {
    if (m_buffer != 0) m_device.destroyBuffer(m_buffer);
    if (m_buffer2 != 0) m_device.destroyBuffer(m_buffer2);
    for (Target* t : {&m_targets[0], &m_targets[1], &m_blurSmall, &m_blurWide}) {
        if (t->texture.id != 0) m_device.destroyTexture(t->texture.id);
    }
    if (m_measure != 0) m_device.destroyTexture(m_measure);
}

void PostEffects::makeTarget(Target& t, uint32_t width, uint32_t height) {
    t.texture.id = m_device.createRenderTarget(width, height, false);
    t.texture.sampler = {2, 2, 2, 3, 3, 0.0f};
    t.width = t.texture.id != 0 ? width : 0;
    t.height = t.texture.id != 0 ? height : 0;
}

bool PostEffects::shaders() const {
    const GraphicsConfig& c = graphicsConfig();
    return c.noShaders == 0 && c.shaders != 0;
}

bool PostEffects::motionBlurOn() const {
    return shaders() && graphicsConfig().motionBlur != 0;
}

bool PostEffects::depthOfFieldOn(const render::Camera& camera, bool always) const {
    return shaders() && graphicsConfig().depthOfField != 0 && (always || camera.depthOfField);
}

void PostEffects::take(const Target* target) {
    const int64_t id = target != nullptr ? static_cast<int64_t>(target->texture.id) : 0;
    if (id == m_taken) return;
    m_taken = id;
    for (uint32_t stage = 0; stage < 4; ++stage) m_device.setTexture(stage, 0);
    m_device.setRenderTarget(static_cast<render::TextureId>(id), 0);
}

void PostEffects::takeTarget(render::TextureId target) {
    const auto id = static_cast<int64_t>(target);
    if (id == m_taken) return;
    m_taken = id;
    for (uint32_t stage = 0; stage < 4; ++stage) m_device.setTexture(stage, 0);
    m_device.setRenderTarget(target, 0);
}

void PostEffects::resume() {
    render::Viewport port;
    if (m_drawing) {
        const Target& frame = m_targets[m_frame];
        take(&frame);
        port.width = frame.width;
        port.height = frame.height;
    } else {
        take(nullptr);
        port.x = static_cast<uint32_t>(m_x0);
        port.y = static_cast<uint32_t>(m_y0);
        port.width = static_cast<uint32_t>(m_x1 - m_x0);
        port.height = static_cast<uint32_t>(m_y1 - m_y0);
    }
    m_device.setViewport(port);
}

void PostEffects::freshState() {
    m_r.endMaterial();
}

void PostEffects::writeVertices(const void* data, uint32_t bytes) {
    m_device.updateVertexBuffer(m_buffer, data, bytes);
}

void PostEffects::begin(const render::Camera& camera, View& view, uint32_t x, uint32_t y, uint32_t width,
                        uint32_t height, bool cutBlend, bool alwaysDepthOfField) {
    const GraphicsConfig& c = graphicsConfig();
    // The window's size changed: the scene's targets made again at it (the stock screen keeps one size).
    if (m_targets[0].texture.id != 0 &&
        (m_targets[0].width != m_device.width() || m_targets[0].height != m_device.height())) {
        for (Target& t : m_targets) {
            m_device.destroyTexture(t.texture.id);
            makeTarget(t, m_device.width(), m_device.height());
        }
        m_taken = -1;
    }
    Target& frame = m_targets[m_frame];
    if (frame.texture.id != 0) {
        bool want = false;
        if (depthOfFieldOn(camera, alwaysDepthOfField)) {
            want = true;
        } else if ((camera.blurEdge != 0.0f || camera.blurMiddle != 0.0f) && motionBlurOn()) {
            want = true;
        } else if ((view.glow && c.noShaders == 0 && c.shaders != 0 && c.glow != 0) || cutBlend) {
            want = true;
        }
        if (want) {
            // 0x00515B70 with 1: +0x200 set, the target taken last forgotten.
            if (!m_drawing) {
                m_drawing = true;
                m_taken = -1;
            }
            // 0x00521170
            if (!m_exposureLoaded) {
                m_exposureProgram = m_engine.program("Material_Exposure");
                m_exposureLoaded = true;
            }
        }
    }
    render::Viewport port;
    if (m_drawing) {
        take(&frame);
        port.width = frame.width;
        port.height = frame.height;
    } else {
        take(nullptr);
        port.x = x;
        port.y = y;
        port.width = width;
        port.height = height;
    }
    m_device.setViewport(port);
    // The view's clear: colour, depth and the stencil the target has (the RT object's +0x18 | 1).
    m_device.clear(ClearFlags::Target | ClearFlags::ZBuffer | ClearFlags::Stencil, view.clear, 1.0f, 0);
    // 0x00516BF0: c22 (2 / width, -2 / height, 0, 0) and c23 (-1, 1, 0, 1), kept for the 2D draws
    // ([0x0079BCF0], [0x0079BCE0]).
    m_viewWidth = width;
    m_viewHeight = height;
    const float c22[4] = {2.0f / static_cast<float>(width), -2.0f / static_cast<float>(height), 0.0f, 0.0f};
    const float c23[4] = {-1.0f, 1.0f, 0.0f, 1.0f};
    m_r.setVertexConstants(22, c22, 1);
    m_r.setVertexConstants(23, c23, 1);
    std::memcpy(m_c22, c22, sizeof(c22));
    std::memcpy(m_c23, c23, sizeof(c23));
    m_x0 = static_cast<float>(x);
    m_y0 = static_cast<float>(y);
    m_x1 = static_cast<float>(x + width);
    m_y1 = static_cast<float>(y + height);
}

void PostEffects::end(const render::Camera& camera, View& view, float cutAmount, bool alwaysDepthOfField,
                      const std::function<void(float, float, float, float)>& smears) {
    const GraphicsConfig& c = graphicsConfig();
    // 0x00516DDD: the scene's target (or the back buffer) taken, the viewport again, no depth test.
    take(m_drawing ? &m_targets[m_frame] : nullptr);
    render::Viewport port;
    port.x = static_cast<uint32_t>(m_x0);
    port.y = static_cast<uint32_t>(m_y0);
    port.width = m_viewWidth;
    port.height = m_viewHeight;
    m_device.setViewport(port);
    m_device.setRenderState(RenderState::ZEnable, 0);
    if (m_drawing) {
        if ((camera.blurEdge != 0.0f || camera.blurMiddle != 0.0f) && motionBlurOn()) {
            motionBlur(camera.blurEdge, camera.blurMiddle, m_x0, m_y0, m_x1, m_y1);
        }
        if (depthOfFieldOn(camera, alwaysDepthOfField)) depthOfField(camera, m_x0, m_y0);
        const bool glowOn = view.glow && c.noShaders == 0 && c.shaders != 0 && c.glow != 0;
        copyToBack(m_x1, m_y1, glowOn);
        if (glowOn) glow(m_x0, m_y0, m_x1, m_y1, view.glowFirst);
    }
    if (cutAmount != 0.0f && m_engine.shaderClasses() != 0x10000) {
        cutBlend(cutAmount, m_x0, m_y0, m_x1, m_y1);
    }
    view.glowFirst = false;
    // 0x0051701E: with the screen's smears (+0x234) the back buffer taken (0x005224E0) and the effects manager's SMEAR
    // emitters drawn over it.
    if (camera.smears && smears) {
        take(nullptr);
        smears(m_x0, m_y0, m_x1, m_y1);
    }
    if (m_drawing) {
        // 0x00515B70 with 0, then the frame's target becomes the previous one.
        m_drawing = false;
        m_taken = -1;
        m_frame = 1 - m_frame;
    }
    // 0x0051708A: the interface's states.
    render::IRenderDevice& d = m_device;
    d.setRenderState(RenderState::ZEnable, 0);
    d.setRenderState(RenderState::AlphaRef, 0x7F);
    d.setRenderState(RenderState::AlphaFunc, CompareFunc::GreaterEqual);
    d.setRenderState(RenderState::FogEnable, 0);
    d.setRenderState(RenderState::ZEnable, 1);
    d.setRenderState(RenderState::AlphaBlendEnable, 1);
    d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
    d.setRenderState(RenderState::DestBlend, BlendFactor::InvSrcAlpha);
    d.setPixelShader(0);
    d.setVertexShader(0);
    d.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::SelectArg1);
    d.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture);
    d.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::SelectArg1);
    d.setTextureStageState(0, TextureStageState::AlphaArg1, TextureArg::Texture);
    d.setTextureStageState(1, TextureStageState::ColourOp, TextureOp::Disable);
    d.setTextureStageState(1, TextureStageState::AlphaOp, TextureOp::Disable);
    d.setTextureStageState(1, TextureStageState::ColourOp, TextureOp::Disable);
    d.setTextureStageState(1, TextureStageState::AlphaOp, TextureOp::Disable);
    d.setTextureStageState(2, TextureStageState::ColourOp, TextureOp::Disable);
    d.setTextureStageState(2, TextureStageState::AlphaOp, TextureOp::Disable);
    freshState();
    m_r.forgetSamplers();
    // The interface's view that follows takes the back buffer (0x0051F660 with the view's target).
    take(nullptr);
}

void PostEffects::motionBlur(float edge, float middle, float x0, float y0, float x1, float y1) {
    freshState();
    take(&m_targets[m_frame]);
    m_motionBlur->setTexture(0, &m_targets[1 - m_frame].texture);
    m_r.beginMaterialPass(*m_motionBlur, 0);
    if (m_r.lastDeclaration != m_declaration2d) {
        m_device.setDeclaration(m_declaration2d);
        m_r.lastDeclaration = m_declaration2d;
    }
    float outer = edge * 1.5f;
    if (0.05f <= outer) {
        if (0.99f < outer) outer = 0.99f;
    } else {
        outer = 0.05f;
    }
    const uint32_t mid = alphaOf(middle * 255.0f) | 0xFFFFFFu;
    const uint32_t ring = alphaOf(outer * 255.0f) | 0xFFFFFFu;
    const uint32_t half = alphaOf((outer + middle) * 128.0f) | 0xFFFFFFu;
    const float cx = (x0 + x1) * 0.5f;
    const float cy = (y0 + y1) * 0.5f;
    Vertex2d v[10] = {
        {cx, cy, mid, 0, 0}, {x0, y0, ring, 0, 0}, {cx, y0, half, 0, 0}, {x1, y0, ring, 0, 0},
        {x1, cy, half, 0, 0}, {x1, y1, ring, 0, 0}, {cx, y1, half, 0, 0}, {x0, y1, ring, 0, 0},
        {x0, cy, half, 0, 0}, {x0, y0, ring, 0, 0},
    };
    // The texture's u v: the place over the view's far corner, drawn 0.985 toward the middle.
    const float invX = 1.0f / x1;
    const float invY = 1.0f / y1;
    for (Vertex2d& p : v) {
        p.u = (invX * p.x - 0.5f) * 0.985f + 0.5f;
        p.v = (invY * p.y - 0.5f) * 0.985f + 0.5f;
    }
    writeVertices(v, sizeof(v));
    m_r.forgetSamplers();
    m_r.applySampler(m_clamped, 0);
    m_device.setRenderState(RenderState::ColourWriteEnable, ColourWrite::Rgb);
    m_device.setStream(m_buffer, sizeof(Vertex2d));
    m_device.draw(Primitive::TriangleFan, 0, 8);
    m_device.setRenderState(RenderState::ColourWriteEnable, ColourWrite::All);
    m_motionBlur->setTexture(0, nullptr);
}

void PostEffects::copyToBack(float width, float height, bool glow) {
    take(&m_targets[m_frame]);
    for (uint32_t stage = 0; stage < 4; ++stage) m_r.applySampler(m_clamped, stage);
    const render::GameTexture& texture = m_targets[m_frame].texture;
    if (m_measure != 0) {
        if (glow) {
            const float half = g_exposure.exposure * 0.5f;
            const float constants[8] = {half, half, half, half, 0.0f, 0.0f, 0.0f, 0.0f};
            m_device.setPixelShaderConstantF(0, constants, 2);
            if (!m_exposureLoaded) {
                m_exposureProgram = m_engine.program("Material_Exposure");
                m_exposureLoaded = true;
            }
            // Stock problem: placed at -0.5 / width, -0.5 / height pixels, not half a pixel.
            drawToBack(texture, -0.5f / width, -0.5f / height, width, height, false, m_exposureProgram);
        } else {
            g_exposure.speed = 0.0f;
            g_exposure.exposure = 1.0f;
            drawToBack(texture, 0.0f, 0.0f, width, height, false, nullptr);
        }
        measureExposure(static_cast<float>(m_targets[m_frame].width), static_cast<float>(m_targets[m_frame].height));
    } else {
        drawToBack(texture, 0.0f, 0.0f, width, height, false, nullptr);
    }
    for (uint32_t stage = 0; stage < 4; ++stage) m_r.applySampler(m_wrapped, stage);
}

void PostEffects::drawToBack(const render::GameTexture& texture, float x, float y, float width, float height,
                             bool alphaOnly, render::Program* program) {
    take(nullptr);
    const uint32_t additive = 0;
    const uint32_t opaque = 1;
    m_texShape->setParam(0, &additive);
    m_texShape->setParam(1, &opaque);
    m_texShape->setTexture(0, &texture);
    m_r.beginMaterialPass(*m_texShape, 0);
    render::IRenderDevice& d = m_device;
    if (alphaOnly) {
        d.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::Disable);
        d.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::SelectArg1);
        d.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture | TextureArg::AlphaReplicate);
        d.setTextureStageState(0, TextureStageState::ColourArg2, TextureArg::Diffuse);
    } else {
        d.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::SelectArg1);
        d.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture);
        d.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::SelectArg1);
        d.setTextureStageState(0, TextureStageState::AlphaArg1, TextureArg::Texture);
        d.setTextureStageState(1, TextureStageState::ColourOp, TextureOp::Disable);
        d.setTextureStageState(1, TextureStageState::AlphaOp, TextureOp::Disable);
        d.setRenderState(RenderState::FogEnable, 0);
    }
    if (m_r.lastDeclaration != m_declaration2d) {
        d.setDeclaration(m_declaration2d);
        m_r.lastDeclaration = m_declaration2d;
    }
    const Vertex2d v[4] = {
        {x, y, 0xFFFFFFFFu, 0.0f, 0.0f},
        {x + width, y, 0xFFFFFFFFu, 1.0f, 0.0f},
        {x, y + height, 0xFFFFFFFFu, 0.0f, 1.0f},
        {x + width, y + height, 0xFFFFFFFFu, 1.0f, 1.0f},
    };
    writeVertices(v, sizeof(v));
    if (program != nullptr) {
        program->beginPass(0);
        m_r.programs().managerVertexConstants(22, m_c22, 1);
        m_r.programs().managerVertexConstants(23, m_c23, 1);
        d.setStream(m_buffer, sizeof(Vertex2d));
        d.draw(Primitive::TriangleStrip, 0, 2);
        program->end();
    } else {
        d.setStream(m_buffer, sizeof(Vertex2d));
        d.draw(Primitive::TriangleStrip, 0, 2);
    }
    m_texShape->setTexture(0, nullptr);
    m_r.endMaterial();
}

void PostEffects::measureExposure(float width, float height) {
    // 0x005213B5: the measure's target taken straight on the device, no depth. Stock problem: the target taken last
    // still holds the back buffer, so the glow's blur over the view and, without the glow, the cut's transition go
    // into the measure.
    m_device.setRenderTarget(m_measure, 0);
    // 0x00520900: the scene's middle (u, v 0.2 .. 0.8) over the whole measure through the luminance.
    render::IRenderDevice& d = m_device;
    d.setTextureStageState(0, TextureStageState::ColourOp, TextureOp::SelectArg1);
    d.setTextureStageState(0, TextureStageState::ColourArg1, TextureArg::Texture);
    d.setTextureStageState(0, TextureStageState::AlphaOp, TextureOp::SelectArg1);
    d.setTextureStageState(0, TextureStageState::AlphaArg1, TextureArg::Texture);
    d.setTextureStageState(1, TextureStageState::ColourOp, TextureOp::Disable);
    d.setTextureStageState(1, TextureStageState::AlphaOp, TextureOp::Disable);
    d.setRenderState(RenderState::FogEnable, 0);
    if (m_r.lastDeclaration != m_declaration2d) {
        d.setDeclaration(m_declaration2d);
        m_r.lastDeclaration = m_declaration2d;
    }
    const Vertex2d v[4] = {
        {0.0f, 0.0f, 0xFFFFFFFFu, 0.2f, 0.2f},
        {width, 0.0f, 0xFFFFFFFFu, 0.8f, 0.2f},
        {0.0f, height, 0xFFFFFFFFu, 0.2f, 0.8f},
        {width, height, 0xFFFFFFFFu, 0.8f, 0.8f},
    };
    writeVertices(v, sizeof(v));
    d.setPixelShader(m_luminance);
    d.setStream(m_buffer, sizeof(Vertex2d));
    d.draw(Primitive::TriangleStrip, 0, 2);
    // 0x00512290: the histogram of the measure's first bytes, normalised.
    std::vector<uint8_t> pixels;
    uint32_t pitch = 0;
    if (!d.readTarget(m_measure, pixels, &pitch)) return;
    ExposureMeasure& m = g_exposure;
    m.histogram.fill(0.0f);
    for (uint32_t row = 0; row < 64; ++row) {
        for (uint32_t column = 0; column < 128; ++column) {
            const uint8_t b = pixels[static_cast<size_t>(row) * pitch + column * 4];
            m.histogram[b] = m.histogram[b] + 1.0f;
        }
    }
    float total = 0.0f;
    for (const float h : m.histogram) total = total + h;
    if (total != 0.0f) {
        const float share = 1.0f / total;
        for (float& h : m.histogram) h = share * h;
    }
    float sum = 0.0f;
    for (int i = 0; i < 256; ++i) {
        sum = sum + m.histogram[static_cast<size_t>(i)];
        if (kLowShare <= sum) {
            m.low = static_cast<float>(i) * kByte;
            break;
        }
    }
    sum = 0.0f;
    for (int i = 255; i >= 0; --i) {
        sum = sum + m.histogram[static_cast<size_t>(i)];
        if (kHighShare <= sum) {
            m.high = static_cast<float>(i) * kByte;
            break;
        }
    }
    float mean = 0.0f;
    for (int i = 0; i < 256; ++i) mean = static_cast<float>(i) * m.histogram[static_cast<size_t>(i)] + mean;
    m.high2 = m.high;
    m.mean = mean * kByte;
    // The speed toward the aim 0.8 of the weighted brightness, half kept, within -0.02 .. 0.02; the exposure
    // within 0.8 .. 2, its speed stopped at the limits.
    const auto weighted = static_cast<float>(static_cast<float>(static_cast<double>(m.high) * 0.7) +
                                              static_cast<float>(static_cast<double>(m.mean) * 0.3));
    const float aim = kExposureAim - weighted * m.exposure;
    m.speed = (1.0f - kExposureKeep) * aim + kExposureKeep * m.speed;
    if (!(m.speed <= kExposureStep)) m.speed = kExposureStep;
    if (!(-kExposureStep <= m.speed)) m.speed = -kExposureStep;
    m.exposure = m.speed + m.exposure;
    if (m.exposure < kExposureMin) {
        m.exposure = kExposureMin;
        if (m.speed < 0.0f) m.speed = 0.0f;
    }
    if (!(m.exposure <= kExposureMax)) {
        m.exposure = kExposureMax;
        if (!(m.speed <= 0.0f)) m.speed = 0.0f;
    }
}

void PostEffects::blurPass(const render::GameTexture& texture) {
    m_blur->setParam(0, &kMinusTwo);
    m_blur->setTexture(0, &texture);
    m_r.beginMaterialPass(*m_blur, 0);
    const float zeros[16] = {};
    m_r.setVertexConstants(52, zeros, 4);
}

// Stock problem: the four u v sets repeat two offsets in some corners and differ between corners.
void PostEffects::blurQuad(uint32_t colour, float x0, float y0, float x1, float y1, float z) {
    constexpr float d = 0.00390625f;   // 1 / 256
    constexpr float lo = 0.99609375f;  // 1 - 1 / 256
    constexpr float hi = 1.00390625f;  // 1 + 1 / 256
    const BlurVertex v[4] = {
        {x0, y0, z, 1.0f, colour, {-d, -d, d, d, -d, -d, d, d}},
        {x1, y0, z, 1.0f, colour, {lo, -d, lo, d, hi, -d, hi, d}},
        {x0, y1, z, 1.0f, colour, {-d, lo, d, lo, -d, hi, d, hi}},
        {x1, y1, z, 1.0f, colour, {lo, lo, lo, lo, hi, hi, hi, hi}},
    };
    m_device.updateVertexBuffer(m_buffer2, v, sizeof(v));
    m_device.setTextureStageState(3, TextureStageState::TextureTransformFlags, TextureTransform::Disable);
    m_device.setFvf(Fvf::XyzRhw | Fvf::Diffuse | Fvf::Tex4);
    m_device.setStream(m_buffer2, sizeof(BlurVertex));
    m_device.draw(Primitive::TriangleStrip, 0, 2);
    m_r.lastDeclaration = 0;
}

// 0x005214C0: the previous frame blurred into the wide blur target, then that target blurred over the frame's at four
// depths around the camera's point: 10 farther (alpha 64, the depth test as it is), then with GREATER 4.5 farther
// (31), 12 and 20 nearer (31, 102); the test LESSEQUAL again.
void PostEffects::depthOfField(const render::Camera& camera, float x0, float y0) {
    m_r.forgetSamplers();
    freshState();
    take(&m_blurWide);
    blurPass(m_targets[1 - m_frame].texture);
    render::IRenderDevice& d = m_device;
    d.setRenderState(RenderState::ColourWriteEnable, ColourWrite::Rgb);
    d.setRenderState(RenderState::AlphaBlendEnable, 0);
    d.setRenderState(RenderState::ZEnable, 0);
    blurQuad(0xFFFFFFFFu, x0, y0, static_cast<float>(m_blurWide.width), static_cast<float>(m_blurWide.height), 0.0f);
    m_r.forgetSamplers();
    freshState();
    const Target& frame = m_targets[m_frame];
    take(&frame);
    blurPass(m_blurWide.texture);
    d.setRenderState(RenderState::AlphaBlendEnable, 1);
    d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
    d.setRenderState(RenderState::DestBlend, BlendFactor::InvSrcAlpha);
    d.setRenderState(RenderState::ZEnable, 1);
    d.setRenderState(RenderState::ZWriteEnable, 0);
    d.setRenderState(RenderState::ColourWriteEnable, ColourWrite::Rgb);
    // The point through the world-to-clip matrix (0x0051F970 with w 1): 1 / w and z / w.
    const Mat44& m = camera.worldToClip();
    const Vec4& p = camera.focus;
    const float clipZ = ((m[8] * p[0] + m[10] * p[2]) + m[9] * p[1]) + m[11];
    const float clipW = ((m[12] * p[0] + m[14] * p[2]) + m[13] * p[1]) + m[15];
    const float inverse = 1.0f / clipW;
    const float depth = inverse * clipZ;
    const auto plane = [&](float offset, float alpha) {
        float z = inverse * offset + depth;
        if (z < 0.0f) {
            z = 0.0f;
        } else if (!(z <= 1.0f)) {
            z = 1.0f;
        }
        blurQuad(alphaOf(alpha) | 0xFFFFFFu, x0, y0, static_cast<float>(frame.width),
                 static_cast<float>(frame.height), z);
    };
    plane(10.0f, 63.75f);
    d.setRenderState(RenderState::ZFunc, CompareFunc::Greater);
    d.setRenderState(RenderState::ZEnable, 1);
    plane(4.5f, 30.6f);
    plane(-12.0f, 30.6f);
    plane(-20.0f, 102.0f);
    d.setRenderState(RenderState::ZFunc, CompareFunc::LessEqual);
    d.setRenderState(RenderState::ZWriteEnable, 1);
    d.setRenderState(RenderState::ColourWriteEnable, ColourWrite::All);
    for (uint32_t stage = 0; stage < 4; ++stage) d.setTexture(stage, 0);
    m_r.forgetSamplers();
    freshState();
    m_taken = -1;
}

// 0x00521C80: after its first frame the small blur target blurred and added over the target taken last (0x9F alpha);
// then the previous frame into the small blur target with the alpha LightBlur x 204, over 0.55 of what it held
// (none in the first frame) where the device has a blend factor, else as it is.
void PostEffects::glow(float x0, float y0, float x1, float y1, bool first) {
    for (uint32_t stage = 0; stage < 4; ++stage) m_r.applySampler(m_clamped, stage);
    take(nullptr);
    m_blur->setParam(0, &kMinusTwo);
    m_blur->setTexture(0, &m_blurSmall.texture);
    render::IRenderDevice& d = m_device;
    if (m_r.lastDeclaration != m_declaration2d) {
        d.setDeclaration(m_declaration2d);
        m_r.lastDeclaration = m_declaration2d;
    }
    if (!first) {
        m_r.beginMaterialPass(*m_blur, 0);
        const uint32_t grey = 0x9F9F9F9Fu;
        const Vertex2d v[4] = {
            {x0, y0, grey, 0.0f, 0.0f},
            {x1, y0, grey, 1.0f, 0.0f},
            {x0, y1, grey, 0.0f, 1.0f},
            {x1, y1, grey, 1.0f, 1.0f},
        };
        writeVertices(v, sizeof(v));
        // [0x007A26C8] and [0x007A2688]: made once.
        static const float kOffsets[16] = {0.005273437f, 0.005273437f, 0, 0, 0.0017578125f, 0.0017578125f, 0, 0,
                                           -0.0017578125f, -0.0017578125f, 0, 0, -0.005273437f, -0.005273437f, 0, 0};
        static const float kWeights[16] = {0.25f, 0.23f, 0.23f, 0.23f, 0.23f, 0.23f, 0.23f, 0.25f,
                                           0.23f, 0.25f, 0.23f, 0.23f, 0.23f, 0.23f, 0.23f, 0.25f};
        m_r.setVertexConstants(52, kOffsets, 4);
        d.setPixelShaderConstantF(0, kWeights, 4);
        d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
        d.setRenderState(RenderState::DestBlend, BlendFactor::One);
        d.setRenderState(RenderState::AlphaBlendEnable, 1);
        d.setRenderState(RenderState::ZEnable, 0);
        d.setStream(m_buffer, sizeof(Vertex2d));
        d.draw(Primitive::TriangleStrip, 0, 2);
    }
    m_blur->setTexture(0, nullptr);
    take(&m_blurSmall);
    const uint32_t opaque = 1;
    const uint32_t additive = 0;
    m_texShape->setTexture(0, &m_targets[1 - m_frame].texture);
    m_texShape->setParam(1, &opaque);
    m_texShape->setParam(0, &additive);
    m_r.beginMaterialPass(*m_texShape, 0);
    const float keep = first ? 0.0f : 0.55f;
    const uint32_t colour = alphaOf(m_r.lightBlur * 204.0f) | 0xFFFFFFu;
    const Vertex2d v[4] = {
        {x0, y0, colour, 0.0f, 0.0f},
        {x1, y0, colour, 1.0f, 0.0f},
        {x0, y1, colour, 0.0f, 1.0f},
        {x1, y1, colour, 1.0f, 1.0f},
    };
    writeVertices(v, sizeof(v));
    d.setRenderState(RenderState::ColourWriteEnable, ColourWrite::All);
    // The device's SrcBlendCaps with D3DPBLENDCAPS_BLENDFACTOR (the renderer's caps, +0x6BD bit 5).
    if ((m_device.sourceBlendCaps() & 0x2000) != 0) {
        d.setRenderState(RenderState::AlphaBlendEnable, 1);
        d.setRenderState(RenderState::SrcBlend, BlendFactor::SrcAlpha);
        d.setRenderState(RenderState::DestBlend, BlendFactor::Constant);
        const uint32_t k = static_cast<uint32_t>(x87::chop(keep * 255.0f));
        d.setRenderState(RenderState::BlendFactor, ((k << 8 | k) << 8 | k) << 8 | k);
    } else {
        d.setRenderState(RenderState::AlphaBlendEnable, 0);
    }
    d.setRenderState(RenderState::ZEnable, 0);
    d.setStream(m_buffer, sizeof(Vertex2d));
    d.draw(Primitive::TriangleStrip, 0, 2);
    const uint32_t blended = 0;
    m_texShape->setParam(1, &blended);
    m_texShape->setTexture(0, nullptr);
    d.setRenderState(RenderState::ColourWriteEnable, ColourWrite::All);
    d.setRenderState(RenderState::ZEnable, 1);
    d.setRenderState(RenderState::ZWriteEnable, 1);
    d.setRenderState(RenderState::AlphaBlendEnable, 0);
    for (uint32_t stage = 0; stage < 4; ++stage) m_r.applySampler(m_wrapped, stage);
}

void PostEffects::cutBlend(float amount, float x0, float y0, float x1, float y1) {
    take(nullptr);
    m_motionBlur->setTexture(0, &m_targets[1 - m_frame].texture);
    m_r.beginMaterialPass(*m_motionBlur, 0);
    if (m_r.lastDeclaration != m_declaration2d) {
        m_device.setDeclaration(m_declaration2d);
        m_r.lastDeclaration = m_declaration2d;
    }
    for (uint32_t stage = 0; stage < 4; ++stage) m_r.applySampler(m_clamped, stage);
    // The middle and the corners by 1 - the amount, the edges' middles by amount x 0.7071 x 255 + amount x 0.2929.
    const float rest = (1.0f - amount) * 255.0f;
    const uint32_t mid = alphaOf(rest);
    const uint32_t corner = alphaOf(rest);
    const uint32_t edge = alphaOf(amount * 0.7071f * 255.0f + amount * 0.2929f);
    const float cx = (x0 + x1) * 0.5f;
    const float cy = (y0 + y1) * 0.5f;
    Vertex2d v[10] = {
        {cx, cy, mid, 0, 0}, {x0, y0, corner, 0, 0}, {cx, y0, edge, 0, 0}, {x1, y0, corner, 0, 0},
        {x1, cy, edge, 0, 0}, {x1, y1, corner, 0, 0}, {cx, y1, edge, 0, 0}, {x0, y1, corner, 0, 0},
        {x0, cy, edge, 0, 0}, {x0, y0, corner, 0, 0},
    };
    const float invX = 1.0f / x1;
    const float invY = 1.0f / y1;
    for (Vertex2d& p : v) {
        p.u = invX * p.x;
        p.v = invY * p.y;
    }
    writeVertices(v, sizeof(v));
    m_device.setStream(m_buffer, sizeof(Vertex2d));
    m_device.draw(Primitive::TriangleFan, 0, 8);
    m_motionBlur->setTexture(0, nullptr);
}

}  // namespace oj::scene
