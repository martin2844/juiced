#include "Render/D3D9/D3D9HeatHaze.h"

#include "Core/Base/Log.h"

#include <algorithm>
#include <cmath>

#include <Shaders/HeatHazeShaders.h>

namespace oj::render::d3d9 {

namespace {

// The passes' settings (JuicedConfigExt's HeatHaze, Shared/Shaders/HeatHaze.hlsl; Game/Scene/HeatHaze has the same).
constexpr float kShift = 6.0f;      // at a depth of 1 cm, at 100 %
constexpr float kNearest = 50.0f;
constexpr float kFarthest = 600.0f;
constexpr float kSway = 0.5f;
constexpr float kNeutral = 0.03f;
constexpr float kLargest = 0.03f;
constexpr UINT kVsParams = 90;
constexpr UINT kMinView = 16;
constexpr uint32_t kMaxQuads = 256 * 4 * 2;  // the particles, the quads between them, twice at a double exhaust

template <class T>
void release(T*& p) {
    if (p != nullptr) {
        p->Release();
        p = nullptr;
    }
}

// The seconds of the performance counter, wrapped every 1000 s (the sway stays precise in a float).
float seconds() {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart == 0) return 0.0f;
    return static_cast<float>(
        std::fmod(static_cast<double>(now.QuadPart) / static_cast<double>(frequency.QuadPart), 1000.0));
}

struct QuadVertex {
    float x, y, z, rhw, u, v;
};

}  // namespace

HeatHaze::HeatHaze(float strength) : m_shiftScale(kShift * strength) {
    m_indices.resize(static_cast<size_t>(kMaxQuads) * 6);
    for (uint32_t q = 0; q < kMaxQuads; ++q) {
        const auto first = static_cast<uint16_t>(q * 4);
        const uint16_t order[6] = {0, 1, 2, 2, 1, 3};
        for (size_t k = 0; k < 6; ++k) m_indices[q * 6 + k] = static_cast<uint16_t>(first + order[k]);
    }
}

void HeatHaze::setStrength(float strength) {
    m_shiftScale = kShift * strength;
}

HeatHaze::~HeatHaze() {
    releaseTargets();
    release(m_vs);
    release(m_offset);
    release(m_composite);
}

void HeatHaze::releaseTargets() {
    release(m_copySurface);
    release(m_copy);
    release(m_shiftSurface);
    release(m_shiftTexture);
    release(m_shiftSamples);
    release(m_states);
    m_width = m_height = 0;
    m_format = D3DFMT_UNKNOWN;
    m_samples = D3DMULTISAMPLE_NONE;
    m_quality = 0;
    m_failed = false;
}

bool HeatHaze::ensure(IDirect3DDevice9* device, const D3DSURFACE_DESC& target) {
    if (m_device != device) {
        releaseTargets();
        release(m_vs);
        release(m_offset);
        release(m_composite);
        m_device = device;
    }
    const bool same = m_width == target.Width && m_height == target.Height && m_format == target.Format &&
                      m_samples == target.MultiSampleType && m_quality == target.MultiSampleQuality;
    if (same && m_copy != nullptr) return true;
    if (same && m_failed) return false;
    releaseTargets();
    m_width = target.Width;
    m_height = target.Height;
    m_format = target.Format;
    m_samples = target.MultiSampleType;
    m_quality = target.MultiSampleQuality;
    const UINT w = target.Width, h = target.Height;
    const bool ok =
        (m_vs != nullptr ||
         SUCCEEDED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_VsHeatHaze), &m_vs))) &&
        (m_offset != nullptr ||
         SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_PsHeatHazeOffset), &m_offset))) &&
        (m_composite != nullptr ||
         SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_PsHeatHazeComposite), &m_composite))) &&
        SUCCEEDED(device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, target.Format, D3DPOOL_DEFAULT, &m_copy,
                                        nullptr)) &&
        SUCCEEDED(m_copy->GetSurfaceLevel(0, &m_copySurface)) &&
        SUCCEEDED(device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                        &m_shiftTexture, nullptr)) &&
        SUCCEEDED(m_shiftTexture->GetSurfaceLevel(0, &m_shiftSurface)) &&
        SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &m_states));
    if (ok && target.MultiSampleType != D3DMULTISAMPLE_NONE &&
        FAILED(device->CreateRenderTarget(w, h, D3DFMT_A8R8G8B8, target.MultiSampleType, target.MultiSampleQuality,
                                          FALSE, &m_shiftSamples, nullptr))) {
        log::warning("heat haze: no multisampled target for the particles, drawn without the depth test");
    }
    if (!ok) {
        log::warning("heat haze: the targets for {}x{} could not be made, no heat haze", w, h);
        releaseTargets();
        m_width = w;
        m_height = h;
        m_format = target.Format;
        m_samples = target.MultiSampleType;
        m_quality = target.MultiSampleQuality;
        m_failed = true;
        return false;
    }
    return true;
}

uint32_t HeatHaze::begin(IDirect3DDevice9* device, IDirect3DSurface9* target, IDirect3DSurface9* depth,
                         const D3DVIEWPORT9& asked) {
    if (device == nullptr || target == nullptr || m_running) return 0;
    D3DSURFACE_DESC desc{};
    target->GetDesc(&desc);
    D3DVIEWPORT9 viewport = asked;
    viewport.Width = std::min<DWORD>(viewport.Width, desc.Width - std::min<DWORD>(viewport.X, desc.Width));
    viewport.Height = std::min<DWORD>(viewport.Height, desc.Height - std::min<DWORD>(viewport.Y, desc.Height));
    const RECT area{static_cast<LONG>(viewport.X), static_cast<LONG>(viewport.Y),
                    static_cast<LONG>(viewport.X + viewport.Width), static_cast<LONG>(viewport.Y + viewport.Height)};
    if (viewport.Width < kMinView || viewport.Height < kMinView || !ensure(device, desc) ||
        FAILED(device->StretchRect(target, &area, m_copySurface, &area, D3DTEXF_NONE))) {
        return 0;
    }
    m_states->Capture();
    m_target = target;
    m_depth = depth;
    m_viewport = viewport;
    m_area = area;
    // The particles' target: with the game's samples when it has them, so the game's depth buffer fits it.
    const bool antialiased = desc.MultiSampleType != D3DMULTISAMPLE_NONE;
    IDirect3DSurface9* particles = antialiased && m_shiftSamples != nullptr ? m_shiftSamples : m_shiftSurface;
    m_depthTest = depth != nullptr && (!antialiased || m_shiftSamples != nullptr);
    const float vw = static_cast<float>(viewport.Width), vh = static_cast<float>(viewport.Height);
    const float screen[4] = {vw * 0.5f / static_cast<float>(desc.Width), -vh * 0.5f / static_cast<float>(desc.Height),
                             0.0f, 0.0f};
    const float params[4] = {seconds(), kSway, vw / vh, kNeutral};
    const float params2[4] = {kLargest, 1.0f / kLargest, 0.0f, 0.0f};
    const float vsParams[4] = {m_shiftScale, kNearest, kFarthest, 0.0f};
    std::copy(screen, screen + 4, m_psParams[0]);
    std::copy(params, params + 4, m_psParams[1]);
    std::copy(params2, params2 + 4, m_psParams[2]);
    std::copy(vsParams, vsParams + 4, m_vsParams);
    m_draws = 0;
    device->SetRenderTarget(0, particles);
    device->SetViewport(&viewport);
    if (!m_depthTest) device->SetDepthStencilSurface(nullptr);
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    m_running = true;
    return viewport.Width;
}

void HeatHaze::draw(IDirect3DDevice9* device, const void* vertices, uint32_t quads, float boost) {
    if (!m_running || vertices == nullptr || quads == 0) return;
    quads = std::min(quads, kMaxQuads);
    device->SetVertexShader(m_vs);
    device->SetPixelShader(m_offset);
    device->SetRenderState(D3DRS_ZENABLE, m_depthTest ? D3DZB_TRUE : D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    const float vsParams[4] = {m_vsParams[0] * boost, m_vsParams[1], m_vsParams[2], m_vsParams[3]};
    device->SetVertexShaderConstantF(kVsParams, vsParams, 1);
    device->SetPixelShaderConstantF(0, &m_psParams[0][0], 3);
    device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, quads * 4, quads * 2, m_indices.data(), D3DFMT_INDEX16,
                                   vertices, 24);
    ++m_draws;
}

void HeatHaze::end(IDirect3DDevice9* device) {
    if (!m_running) return;
    m_running = false;
    device->SetRenderTarget(0, m_target);
    device->SetDepthStencilSurface(m_depth);
    device->SetViewport(&m_viewport);
    if (m_draws > 0) {
        if (m_shiftSamples != nullptr && m_samples != D3DMULTISAMPLE_NONE) {
            device->StretchRect(m_shiftSamples, &m_area, m_shiftSurface, &m_area, D3DTEXF_NONE);
        }
        // The copy of the frame over the view, each pixel moved by the shift stored for it.
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->SetPixelShader(m_composite);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,
                               D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        const DWORD filters[2] = {D3DTEXF_POINT, D3DTEXF_LINEAR};
        IDirect3DTexture9* const textures[2] = {m_shiftTexture, m_copy};
        for (DWORD stage = 0; stage < 2; ++stage) {
            device->SetTexture(stage, textures[stage]);
            device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filters[stage]);
            device->SetSamplerState(stage, D3DSAMP_MINFILTER, filters[stage]);
            device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        }
        device->SetPixelShaderConstantF(2, m_psParams[2], 1);
        // The view, half a pixel shifted: the texel centres of both textures meet the pixel centres.
        const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
        const float x0 = static_cast<float>(m_viewport.X), y0 = static_cast<float>(m_viewport.Y);
        const float x1 = x0 + static_cast<float>(m_viewport.Width), y1 = y0 + static_cast<float>(m_viewport.Height);
        const QuadVertex quad[4] = {{x0 - 0.5f, y0 - 0.5f, 0.0f, 1.0f, x0 / w, y0 / h},
                                    {x1 - 0.5f, y0 - 0.5f, 0.0f, 1.0f, x1 / w, y0 / h},
                                    {x0 - 0.5f, y1 - 0.5f, 0.0f, 1.0f, x0 / w, y1 / h},
                                    {x1 - 0.5f, y1 - 0.5f, 0.0f, 1.0f, x1 / w, y1 / h}};
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));
        if (!m_logged) {
            m_logged = true;
            log::info("heat haze: the first drawn ({} draws, view {}x{}, {})", m_draws, m_viewport.Width,
                      m_viewport.Height, m_depthTest ? "depth tested" : "no depth test");
        }
    }
    m_states->Apply();  // the game's states, shaders, textures and viewport
    m_target = nullptr;
    m_depth = nullptr;
}

}  // namespace oj::render::d3d9
