#include "Render/D3D9/D3D9Device.h"

#include "Render/D3D9/D3D9HeatHaze.h"
#include "Render/D3D9/D3D9Profiler.h"

#include "Core/Base/LoadingThread.h"
#include "Core/Base/Log.h"
#include "Render/Display.h"
#include "Render/FrameOptions.h"
#include "Render/Program.h"
#include "Render/ShaderReplacements.h"

#include <windows.h>
#include <d3d9.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <format>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <BrightnessPass.h>
#include <CloudPass.h>
#include <DepthDilatePass.h>
#include <AmbientOcclusionPass.h>
#include <LampShadowPass.h>
#include <PointLightPass.h>
#include <VolumetricLightPass.h>
#include <LightReflectionPass.h>
#include <SmaaPass.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace oj::render::d3d9 {

namespace {

template <class T>
void release(T*& p) {
    if (p != nullptr) {
        p->Release();
        p = nullptr;
    }
}

// Vertex of the debug triangles: pre-transformed position (pixels, rhw) and diffuse colour.
struct ScreenVertex {
    float x, y, z, rhw;
    D3DCOLOR color;
};
constexpr DWORD kScreenVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

// Candidate formats in the order the stock renderer tries them.
constexpr D3DFORMAT kBackBuffer32[] = {D3DFMT_X8R8G8B8, D3DFMT_A2R10G10B10, D3DFMT_A8R8G8B8, D3DFMT_X8B8G8R8,
                                       D3DFMT_R5G6B5};
constexpr D3DFORMAT kBackBuffer16[] = {D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_X8R8G8B8, D3DFMT_A2R10G10B10,
                                       D3DFMT_A8R8G8B8, D3DFMT_X8B8G8R8};
constexpr D3DFORMAT kDepth32[] = {D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D24X4S4, D3DFMT_D16};
constexpr D3DFORMAT kDepth16[] = {D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D24X4S4, D3DFMT_D16, D3DFMT_D15S1};

bool hasStencil(D3DFORMAT f) {
    return f == D3DFMT_D24S8 || f == D3DFMT_D24X4S4 || f == D3DFMT_D15S1;
}

const char* formatName(D3DFORMAT f) {
    switch (f) {
    case D3DFMT_X8R8G8B8: return "X8R8G8B8";
    case D3DFMT_A8R8G8B8: return "A8R8G8B8";
    case D3DFMT_A2R10G10B10: return "A2R10G10B10";
    case D3DFMT_X8B8G8R8: return "X8B8G8R8";
    case D3DFMT_R5G6B5: return "R5G6B5";
    case D3DFMT_X1R5G5B5: return "X1R5G5B5";
    case D3DFMT_D24S8: return "D24S8";
    case D3DFMT_D24X8: return "D24X8";
    case D3DFMT_D24X4S4: return "D24X4S4";
    case D3DFMT_D16: return "D16";
    case D3DFMT_D15S1: return "D15S1";
    default: return "?";
    }
}

// Where the levels of texture data start; levels is 0 when not even the first fits.
struct Layout {
    D3DFORMAT format = D3DFMT_A8R8G8B8;
    uint32_t blockBytes = 0;  // 0: texelBytes a pixel
    uint32_t texelBytes = 4;
    uint32_t levels = 0;
    std::vector<size_t> starts;
    size_t faceBytes = 0;
};

// The levels of `faces` faces stored one face after another: one face takes the levels whose bytes are present, a
// cube all of them or none.
Layout layoutOf(const TextureData& data, uint32_t faces) {
    Layout out;
    switch (data.format) {
    case PixelFormat::DXT1: out.format = D3DFMT_DXT1; out.blockBytes = 8; break;
    case PixelFormat::DXT3: out.format = D3DFMT_DXT3; out.blockBytes = 16; break;
    case PixelFormat::DXT5: out.format = D3DFMT_DXT5; out.blockBytes = 16; break;
    case PixelFormat::A8R8G8B8: out.format = D3DFMT_A8R8G8B8; break;
    case PixelFormat::X8R8G8B8: out.format = D3DFMT_X8R8G8B8; break;
    case PixelFormat::R5G6B5: out.format = D3DFMT_R5G6B5; out.texelBytes = 2; break;
    }
    uint32_t present = 0;
    for (uint32_t l = 0; l < std::max(data.levels, 1u); ++l) {
        const uint32_t w = std::max(1u, data.width >> l);
        const uint32_t h = std::max(1u, data.height >> l);
        const size_t bytes =
            out.blockBytes != 0 ? size_t((w + 3) / 4) * ((h + 3) / 4) * out.blockBytes : size_t(w) * h * out.texelBytes;
        out.starts.push_back(out.faceBytes);
        out.faceBytes += bytes;
        if (out.faceBytes <= data.size) ++present;
    }
    if (faces == 1) {
        out.levels = present;
    } else if (out.faceBytes * faces <= data.size) {
        out.levels = static_cast<uint32_t>(out.starts.size());
    }
    return out;
}

void copyLevel(const Layout& layout, const TextureData& data, uint32_t level, size_t start,
               const D3DLOCKED_RECT& locked) {
    const uint32_t w = std::max(1u, data.width >> level);
    const uint32_t h = std::max(1u, data.height >> level);
    const uint32_t rows = layout.blockBytes != 0 ? (h + 3) / 4 : h;
    const size_t rowBytes =
        layout.blockBytes != 0 ? size_t((w + 3) / 4) * layout.blockBytes : size_t(w) * layout.texelBytes;
    for (uint32_t r = 0; r < rows; ++r) {
        std::memcpy(static_cast<uint8_t*>(locked.pBits) + size_t(r) * locked.Pitch, data.pixels + start + r * rowBytes,
                    rowBytes);
    }
}

class Device final : public IRenderDevice {
public:
    ~Device() override {
        m_heatHaze.reset();
        m_brightness.reset();
        m_smaa.reset();
        m_occlusion.reset();
        m_reflections.reset();
        m_lampShadows.reset();
        m_volumetric.reset();
        m_pointLights.reset();
        m_clouds.reset();
        m_dilate.reset();
        release(m_backColor);
        release(m_backDepth);
        releaseFrame();
        for (auto& [key, depth] : m_depths) release(depth);
        releaseSamples();
        for (IDirect3DBaseTexture9*& texture : m_textures) {
            release(texture);
        }
        for (IUnknown*& shader : m_shaders) {
            release(shader);
        }
        for (IDirect3DVertexDeclaration9*& declaration : m_declarations) {
            release(declaration);
        }
        for (IDirect3DResource9*& buffer : m_buffers) {
            release(buffer);
        }
        for (IDirect3DQuery9*& query : m_queries) {
            release(query);
        }
        release(m_device);
        release(m_d3d);
    }

    bool init(const DeviceDesc& desc, std::string* error) {
        m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (m_d3d == nullptr) {
            *error = "Direct3DCreate9 failed (no Direct3D 9 runtime)";
            return false;
        }
        m_adapterIndex = desc.adapter < m_d3d->GetAdapterCount() ? desc.adapter : 0;
        D3DADAPTER_IDENTIFIER9 id{};
        if (SUCCEEDED(m_d3d->GetAdapterIdentifier(m_adapterIndex, 0, &id))) {
            m_adapter = id.Description;
            m_info.driverProduct = HIWORD(id.DriverVersion.HighPart);
            m_info.driverVersion = LOWORD(id.DriverVersion.HighPart);
            m_info.driverSubVersion = HIWORD(id.DriverVersion.LowPart);
            m_info.driverBuild = LOWORD(id.DriverVersion.LowPart);
            m_info.vendorId = id.VendorId;
            m_info.deviceId = id.DeviceId;
            m_info.subSysId = id.SubSysId;
            m_info.revision = id.Revision;
        }
        RECT client{};
        GetClientRect(desc.window, &client);
        m_pp = {};
        m_pp.BackBufferWidth = static_cast<UINT>(std::max<LONG>(client.right - client.left, 1));
        m_pp.BackBufferHeight = static_cast<UINT>(std::max<LONG>(client.bottom - client.top, 1));
        m_pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        m_pp.hDeviceWindow = desc.window;
        m_pp.Windowed = desc.windowed ? TRUE : FALSE;
        m_pp.EnableAutoDepthStencil = TRUE;
        m_pp.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
        m_pp.PresentationInterval = desc.presentIntervalOne ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_DEFAULT;
        if (!chooseFormats(desc, error)) {
            return false;
        }
        // The fixed build's screen modes: the full screen mode's refresh rate; a picture of another size than the
        // window without a frame drawn into a surface of its own, the multisampling the surface's (not the back
        // buffer's).
        if (!desc.windowed) m_pp.FullScreen_RefreshRateInHz = fullScreenRate(desc.refreshRate);
        if (desc.windowed && desc.pictureWidth != 0 && desc.pictureHeight != 0 &&
            (desc.pictureWidth != m_pp.BackBufferWidth || desc.pictureHeight != m_pp.BackBufferHeight)) {
            m_frameWidth = desc.pictureWidth;
            m_frameHeight = desc.pictureHeight;
            m_frameSamples = m_pp.MultiSampleType;
            m_frameQuality = m_pp.MultiSampleQuality;
            m_pp.MultiSampleType = D3DMULTISAMPLE_NONE;
            m_pp.MultiSampleQuality = 0;
        }
        m_stretch = desc.stretch;

        D3DCAPS9 caps{};
        m_d3d->GetDeviceCaps(m_adapterIndex, D3DDEVTYPE_HAL, &caps);
        // The fixed build: VSync off presents at once when the adapter can; the texture filter and LOD bias (the
        // adapter's anisotropy at most); the frame rate counter.
        if (!desc.vsync && (caps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE) != 0) {
            m_pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        }
        m_textureFilter = desc.textureFilter;
        m_lodBias = desc.lodBias;
        m_maxAnisotropy = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0 ? caps.MaxAnisotropy : 0;
        m_quality = TextureQuality::make(m_textureFilter, m_lodBias, m_maxAnisotropy != 0, m_maxAnisotropy);
        m_msaa = desc.msaa;
        m_showFps = desc.showFps;
        if (desc.worldLight) {
            m_replacements.features |= static_cast<unsigned>(ShaderFeature::WorldLight);
            m_replacements.worldLightStrength = desc.worldLightStrength;
        }
        if (desc.headlights) m_replacements.features |= static_cast<unsigned>(ShaderFeature::Headlights);
        if (desc.sunShadows) m_replacements.features |= static_cast<unsigned>(ShaderFeature::SunShadows);
        m_replacements.shaders20 =
            (shaderClasses(caps.VertexShaderVersion, caps.PixelShaderVersion, desc.shaders) & 0x40000u) != 0;
        constexpr DWORD kLinear = D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFLINEAR;
        m_pictureFilter = (caps.StretchRectFilterCaps & kLinear) == kLinear ? D3DTEXF_LINEAR : D3DTEXF_NONE;
        m_vertexShaderVersion = caps.VertexShaderVersion;
        m_sourceBlendCaps = caps.SrcBlendCaps;
        m_fullScreenGamma = (caps.Caps2 & D3DCAPS2_FULLSCREENGAMMA) != 0;
        m_textureCaps = caps.TextureCaps;
        m_pixelShaderVersion = caps.PixelShaderVersion;
        DWORD flags = 0;
        if (D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) > 1) {
            flags = desc.pureDevice ? (D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE)
                                    : D3DCREATE_HARDWARE_VERTEXPROCESSING;
        } else {
            flags = D3DCREATE_MIXED_VERTEXPROCESSING;
        }
        if (desc.softwareVertexProcessing || (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0) {
            flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }
        if (desc.multithreaded) {
            flags |= D3DCREATE_MULTITHREADED;
        }
        m_flags = flags;
        if (desc.heatHaze) {
            if ((flags & D3DCREATE_PUREDEVICE) == 0) {
                m_heatHaze = std::make_unique<d3d9::HeatHaze>(desc.heatHazeStrength);
            } else {
                log::warning("d3d9: a pure device, no heat haze");
            }
        }
        if (desc.brightness) {
            if ((flags & D3DCREATE_PUREDEVICE) == 0) {
                m_brightness = std::make_unique<BrightnessPass>();
            } else {
                log::warning("d3d9: a pure device, no brightness");
            }
        }
        m_smaaPossible = (flags & D3DCREATE_PUREDEVICE) == 0 && SmaaPass::Supported(caps);
        m_occlusionPossible = (flags & D3DCREATE_PUREDEVICE) == 0 && AmbientOcclusionPass::Supported(caps);
        if (desc.smaa) {
            if ((flags & D3DCREATE_PUREDEVICE) != 0) {
                log::warning("d3d9: a pure device, no SMAA");
            } else if (!SmaaPass::Supported(caps)) {
                log::warning("d3d9: no shaders 3.0, no SMAA");
            } else {
                m_smaa = std::make_unique<SmaaPass>();
            }
        }

        // 1.01 (0x00514EBB): the device asked for up to 250 times, 40 ms (Sleep) after both of a turn's tries failed;
        // the depth format D16 from the first failure on.
        HRESULT hr = D3DERR_NOTAVAILABLE;
        for (int tries = 0; tries < 250; ++tries) {
            hr = m_d3d->CreateDevice(m_adapterIndex, D3DDEVTYPE_HAL, desc.window, flags, &m_pp, &m_device);
            if (FAILED(hr)) {
                m_pp.AutoDepthStencilFormat = D3DFMT_D16;
                hr = m_d3d->CreateDevice(m_adapterIndex, D3DDEVTYPE_HAL, desc.window, flags, &m_pp, &m_device);
                if (FAILED(hr)) Sleep(40);
            }
            if (SUCCEEDED(hr)) break;
        }
        if (FAILED(hr)) {
            *error = std::format("IDirect3D9::CreateDevice failed (0x{:08X})", static_cast<uint32_t>(hr));
            return false;
        }
        m_clearFlags = D3DCLEAR_ZBUFFER | (hasStencil(m_pp.AutoDepthStencilFormat) ? D3DCLEAR_STENCIL : 0);
        makeFrame();
        clearScreen();
        applyTextureQuality(false);
        if (m_quality.any()) {
            log::info("d3d9: textures {} (anisotropy {}), LOD bias {}", m_quality.filter ? "filtered" : "the game's",
                      m_quality.anisotropy, m_quality.bias ? std::format("{:.1f}", m_quality.lodBias) : "the game's");
        }
        return true;
    }

    // The texture filter and LOD bias on every sampler as made or reset (the plugin's TextureQuality::Apply), so that
    // the samplers the game never sets have them too; `always`: with neither (the filter turned off), the game's
    // values back.
    void applyTextureQuality(bool always) {
        if (!always && !m_quality.any()) return;
        for (DWORD s = 0; s < kSamplers; ++s) {
            for (size_t k = 0; k < kQualityStates.size(); ++k) {
                m_device->SetSamplerState(s, static_cast<D3DSAMPLERSTATETYPE>(kQualityStates[k]),
                                          m_quality.adjust(kQualityStates[k], m_gameSamplers[s][k]));
            }
        }
    }

    // The states of the texture filter and the LOD bias the game set last on each sampler: Direct3D's defaults as the
    // device is made or reset.
    void defaultSamplers() {
        for (auto& states : m_gameSamplers) states = kQualityDefaults;
    }

    // The fixed build's MSAA: the most samples up to `msaa` (16 at most) both the frame's colour and depth formats
    // take, at their highest quality; none below 2 or when they take none.
    std::pair<D3DMULTISAMPLE_TYPE, DWORD> msaaSamples(uint32_t msaa) const {
        for (DWORD samples = std::min<DWORD>(msaa, 16); samples >= 2; --samples) {
            DWORD levels = 0;
            const auto type = static_cast<D3DMULTISAMPLE_TYPE>(samples);
            if (SUCCEEDED(m_d3d->CheckDeviceMultiSampleType(m_adapterIndex, D3DDEVTYPE_HAL, m_pp.BackBufferFormat,
                                                            m_pp.Windowed, type, &levels)) &&
                levels != 0 &&
                SUCCEEDED(m_d3d->CheckDeviceMultiSampleType(m_adapterIndex, D3DDEVTYPE_HAL,
                                                            m_pp.AutoDepthStencilFormat, m_pp.Windowed, type,
                                                            nullptr))) {
                return {type, levels - 1};
            }
        }
        return {D3DMULTISAMPLE_NONE, 0};
    }

    // The refresh rate of the full screen mode (the back buffer's format and size): the one asked for when the adapter
    // has it, else the highest (none: 0, the adapter's default).
    UINT fullScreenRate(uint32_t wanted) const {
        UINT highest = 0;
        bool exact = false;
        for (UINT i = 0; i < m_d3d->GetAdapterModeCount(m_adapterIndex, m_pp.BackBufferFormat); ++i) {
            D3DDISPLAYMODE mode{};
            if (FAILED(m_d3d->EnumAdapterModes(m_adapterIndex, m_pp.BackBufferFormat, i, &mode)) ||
                mode.Width != m_pp.BackBufferWidth || mode.Height != m_pp.BackBufferHeight) {
                continue;
            }
            highest = std::max(highest, mode.RefreshRate);
            exact = exact || mode.RefreshRate == wanted;
        }
        return exact && wanted != 0 ? wanted : highest;
    }

    // The frame surface (m_frameWidth): made with the device and again after each reset, the game's target 0 (its
    // colour and depth; no surface: the back buffer at its size, the multisampling lost).
    void makeFrame() {
        if (m_frameWidth == 0) return;
        const bool made =
            SUCCEEDED(m_device->CreateRenderTarget(m_frameWidth, m_frameHeight, m_pp.BackBufferFormat, m_frameSamples,
                                                   m_frameQuality, FALSE, &m_frameColor, nullptr)) &&
            SUCCEEDED(m_device->CreateDepthStencilSurface(m_frameWidth, m_frameHeight, m_pp.AutoDepthStencilFormat,
                                                          m_frameSamples, m_frameQuality, TRUE, &m_frameDepth,
                                                          nullptr)) &&
            (m_frameSamples == D3DMULTISAMPLE_NONE ||
             SUCCEEDED(m_device->CreateRenderTarget(m_frameWidth, m_frameHeight, m_pp.BackBufferFormat,
                                                    D3DMULTISAMPLE_NONE, 0, FALSE, &m_frameResolve, nullptr)));
        if (!made) {
            log::warning("d3d9: no {}x{} picture surface, the picture at the window's {}x{}", m_frameWidth,
                         m_frameHeight, m_pp.BackBufferWidth, m_pp.BackBufferHeight);
            releaseFrame();
            m_frameWidth = m_frameHeight = 0;
            return;
        }
        release(m_backColor);
        release(m_backDepth);
        m_backColor = m_frameColor;
        m_backColor->AddRef();
        m_backDepth = m_frameDepth;
        m_backDepth->AddRef();
        m_device->SetRenderTarget(0, m_backColor);
        m_device->SetDepthStencilSurface(m_backDepth);
        const PictureRect r = placePicture(m_pp.BackBufferWidth, m_pp.BackBufferHeight, m_frameWidth, m_frameHeight,
                                           m_stretch);
        m_picture = {r.x0, r.y0, r.x1, r.y1};
        m_pictureInset = r.x0 != 0 || r.y0 != 0 || r.x1 != static_cast<int32_t>(m_pp.BackBufferWidth) ||
                         r.y1 != static_cast<int32_t>(m_pp.BackBufferHeight);
        log::info("d3d9: the picture {}x{} at {},{} {}x{} of the window", m_frameWidth, m_frameHeight, r.x0, r.y0,
                  r.x1 - r.x0, r.y1 - r.y0);
    }

    void releaseFrame() {
        release(m_frameColor);
        release(m_frameDepth);
        release(m_frameResolve);
    }

    // The frame surface into the back buffer as the frame ends: resolved first when multisampled, the back buffer
    // black around the picture, the picture stretched into its place.
    void presentFrame() {
        if (m_frameColor == nullptr) return;
        IDirect3DSurface9* back = nullptr;
        if (FAILED(m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) return;
        IDirect3DSurface9* source = m_frameColor;
        if (m_frameResolve != nullptr &&
            SUCCEEDED(m_device->StretchRect(m_frameColor, nullptr, m_frameResolve, nullptr, D3DTEXF_NONE))) {
            source = m_frameResolve;
        }
        if (m_pictureInset) m_device->ColorFill(back, nullptr, D3DCOLOR_XRGB(0, 0, 0));
        m_device->StretchRect(source, nullptr, back, &m_picture, m_pictureFilter);
        back->Release();
    }

    void antialias() override {
        if (m_smaa == nullptr || m_target != 0) return;
        if (m_backColor == nullptr) {
            m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_backColor);
            m_device->GetDepthStencilSurface(&m_backDepth);
        }
        const D3DVIEWPORT9 full{0, 0, screenWidth(), screenHeight(), 0.0f, 1.0f};
        if (m_smaa->Run(m_device, m_backColor, m_viewportSet ? m_viewport : full) && !m_smaaLogged) {
            m_smaaLogged = true;
            log::info("d3d9: SMAA drawn");
        }
    }

    // The heat haze over the view drawn now: into the back buffer, or into a scene target (the stock's effects draw
    // the scene there: its samples and their depth while bound, else its texture's surface and its depth); none over
    // another target (a mirror, a reflection's).
    uint32_t beginHeatHaze() override {
        if (m_heatHaze == nullptr) return 0;
        IDirect3DSurface9* colour = nullptr;
        IDirect3DSurface9* depth = nullptr;
        D3DVIEWPORT9 full{0, 0, screenWidth(), screenHeight(), 0.0f, 1.0f};
        if (m_target == 0) {
            if (m_backColor == nullptr) {
                m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_backColor);
                m_device->GetDepthStencilSurface(&m_backDepth);
            }
            colour = m_backColor;
            depth = m_backDepth;
        } else {
            const Target* t = targetOf(m_target);
            if (t == nullptr || !t->multisampled || m_textures[m_target - 1] == nullptr) return 0;
            if (m_samplesBound == m_target) {
                const Samples* samples = samplesOf(*t);
                if (samples == nullptr) return 0;
                colour = samples->colour;
                depth = samples->depth;
            } else {
                static_cast<IDirect3DTexture9*>(m_textures[m_target - 1])->GetSurfaceLevel(0, &m_hazeSurface);
                colour = m_hazeSurface;
                depth = depthOf(t->width, t->height);
            }
            full.Width = t->width;
            full.Height = t->height;
        }
        const uint32_t width = m_heatHaze->begin(m_device, colour, depth, m_viewportSet ? m_viewport : full);
        if (width == 0) release(m_hazeSurface);
        return width;
    }
    void drawHeatHaze(const void* vertices, uint32_t quads, float boost) override {
        if (m_heatHaze != nullptr) m_heatHaze->draw(m_device, vertices, quads, boost);
    }
    void endHeatHaze() override {
        if (m_heatHaze != nullptr) m_heatHaze->end(m_device);
        release(m_hazeSurface);
    }

    // A replacement made: logged once a shader.
    void noteReplaced(const char* name) {
        if (name != nullptr && m_replaced.insert(name).second) log::info("d3d9: {} replaced", name);
    }

    // The game's screen: the picture's surface, else the back buffer.
    uint32_t screenWidth() const { return m_frameColor != nullptr ? m_frameWidth : m_pp.BackBufferWidth; }
    uint32_t screenHeight() const { return m_frameColor != nullptr ? m_frameHeight : m_pp.BackBufferHeight; }

    // The frame rate counter (Render/FrameOptions) on the back buffer, after the scene's end, before the present: its
    // rectangles cleared in their colours over the whole back buffer's viewport, no scissor; both put back after.
    void drawFrameRate() {
        if (!m_showFps || m_target != 0) return;
        LARGE_INTEGER now{}, frequency{};
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        m_frameRate.frame(static_cast<double>(now.QuadPart) / static_cast<double>(frequency.QuadPart));
        const std::vector<FpsRect> rects = fpsCounterRects(m_frameRate.shown(), screenHeight());
        const D3DVIEWPORT9 full{0, 0, screenWidth(), screenHeight(), 0.0f, 1.0f};
        m_device->SetViewport(&full);
        if (m_scissor) m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        std::vector<D3DRECT> group;
        for (size_t i = 0; i < rects.size(); ++i) {
            group.push_back({rects[i].x0, rects[i].y0, rects[i].x1, rects[i].y1});
            if (i + 1 == rects.size() || rects[i + 1].argb != rects[i].argb) {
                m_device->Clear(static_cast<DWORD>(group.size()), group.data(), D3DCLEAR_TARGET, rects[i].argb, 1.0f, 0);
                group.clear();
            }
        }
        m_device->SetViewport(m_viewportSet ? &m_viewport : &full);
        if (m_scissor) m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    }

    Backend backend() const override { return Backend::D3D9; }
    std::string adapterName() const override { return m_adapter; }

    std::string description() const override {
        return std::format("{}x{} {} {} depth {}, multisample {}/{}, flags 0x{:X}{}", m_pp.BackBufferWidth,
                           m_pp.BackBufferHeight, m_pp.Windowed ? "windowed" : "full screen",
                           formatName(m_pp.BackBufferFormat), formatName(m_pp.AutoDepthStencilFormat),
                           static_cast<int>(m_frameWidth != 0 ? m_frameSamples : m_pp.MultiSampleType),
                           m_frameWidth != 0 ? m_frameQuality : m_pp.MultiSampleQuality, m_flags,
                           m_frameWidth != 0 ? std::format(", the picture {}x{}", m_frameWidth, m_frameHeight) : "");
    }

    bool beginFrame() override {
        // The frame holds the device to its end; a frame that does not begin lets it go at once. The loading thread
        // waiting for the device goes first (a frame paced by the present has little time between frames).
        if (m_waiting > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_deviceLock.lock();
        if (!restore()) {
            m_deviceLock.unlock();
            return false;
        }
        // The fixed build's wide screens: the colour cleared too (black), as the windows no longer cover the whole
        // screen (a picture kept to 16:9, a menu's pieces in the frame); the stock's frames keep their colour.
        const bool wide = !render::display(screenWidth(), screenHeight()).stock;
        m_device->Clear(0, nullptr, m_clearFlags | (wide ? D3DCLEAR_TARGET : 0), 0xFF000000, 1.0f, 0);
        const HRESULT hr = m_device->BeginScene();
        if (FAILED(hr)) {
            failed("BeginScene", hr);
            m_deviceLock.unlock();
            return false;
        }
        m_inFrame = true;
        ++m_services;  // 0x00514C96
        m_profiler.beginFrame();
        return true;
    }

    void endFrame() override {
        m_profiler.mark("present");
        resolveSamples();
        m_device->EndScene();
        drawFrameRate();
        presentFrame();
        // The brightness over the whole back buffer (the picture and the black around it), a scene of its own.
        if (m_brightness != nullptr && m_brightness->Draw(m_device, m_brightnessLevel) && !m_brightnessLogged) {
            m_brightnessLogged = true;
            log::info("d3d9: the brightness drawn ({:.2f})", m_brightnessLevel);
        }
        const HRESULT hr = m_device->Present(nullptr, nullptr, nullptr, nullptr);
        if (hr == D3DERR_DEVICELOST) {
            log::warning("d3d9: the device is lost");
            m_lost = true;
        }
        m_profiler.endFrame();
        m_inFrame = false;
        m_deviceLock.unlock();
    }

    bool setProfiling(bool on) override { return m_profiler.setOn(m_device, on); }
    void profileMark(const char* layer) override { m_profiler.mark(layer); }
    std::string profileReport() override { return m_profiler.report(); }

    void letLoadingRun() override {
        if (!onDeviceThread()) return;
        ++m_services;  // 0x0045A38A
        if (!m_inFrame) {
            frameTurns().pause();
            return;
        }
        m_deviceLock.unlock();
        frameTurns().pause();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_deviceLock.lock();
    }

    void waitForService() override {
        if (onDeviceThread()) return;
        const uint64_t before = m_services;
        while (m_services == before) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void resize(uint32_t width, uint32_t height) override {
        if (!m_pp.Windowed || width == 0 || height == 0 ||
            (width == m_pp.BackBufferWidth && height == m_pp.BackBufferHeight)) {
            return;
        }
        log::info("d3d9: the window's client is {}x{}", width, height);
        m_pp.BackBufferWidth = width;
        m_pp.BackBufferHeight = height;
        m_resetPending = true;
    }

    void drawDebugTriangles(const DebugVertex* vertices, uint32_t count) override {
        if (count < 3) {
            return;
        }
        std::vector<ScreenVertex> screen(count);
        for (uint32_t i = 0; i < count; ++i) {
            screen[i] = {vertices[i].x, vertices[i].y, 0.0f, 1.0f, vertices[i].argb};
        }
        m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        m_device->SetTexture(0, nullptr);
        m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        m_device->SetFVF(kScreenVertexFvf);
        m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, screen.data(), sizeof(ScreenVertex));
    }

    uint32_t width() const override { return screenWidth(); }
    uint32_t height() const override { return screenHeight(); }
    uint32_t vertexShaderVersion() const override { return m_vertexShaderVersion; }
    uint32_t pixelShaderVersion() const override { return m_pixelShaderVersion; }
    AdapterInfo adapterInfo() const override { return m_info; }
    uint32_t backBufferFormat() const override { return static_cast<uint32_t>(m_pp.BackBufferFormat); }
    uint32_t sourceBlendCaps() const override { return m_sourceBlendCaps; }
    bool fullScreenGamma() const override { return m_fullScreenGamma; }

    void setAntialiasing(uint32_t msaa, bool smaa) override {
        if (smaa && m_smaa == nullptr && m_smaaPossible) {
            m_smaa = std::make_unique<SmaaPass>();
            log::info("d3d9: SMAA on");
        } else if (!smaa && m_smaa != nullptr) {
            m_smaa.reset();
            log::info("d3d9: SMAA off");
        }
        if (msaa == m_msaa) return;
        m_msaa = msaa;
        const auto [type, quality] = msaaSamples(msaa);
        if (type == frameSamples() && quality == frameQuality()) return;
        if (m_frameWidth != 0) {
            m_frameSamples = type;
            m_frameQuality = quality;
        } else {
            m_pp.MultiSampleType = type;
            m_pp.MultiSampleQuality = quality;
        }
        m_resetPending = true;
        log::info("d3d9: MSAA {} samples", static_cast<int>(type));
    }

    void setTextureFilter(uint32_t filter) override {
        if (filter == m_textureFilter) return;
        m_textureFilter = filter;
        m_quality = TextureQuality::make(m_textureFilter, m_lodBias, m_maxAnisotropy != 0, m_maxAnisotropy);
        applyTextureQuality(true);
        log::info("d3d9: textures {} (anisotropy {})", m_quality.filter ? "filtered" : "the game's",
                  m_quality.anisotropy);
    }

    QualityCaps qualityCaps() const override {
        QualityCaps caps;
        for (uint32_t samples = 2; samples <= 16; ++samples) {
            if (msaaSamples(samples).first == static_cast<D3DMULTISAMPLE_TYPE>(samples)) {
                caps.multisampleCounts.push_back(samples);
            }
        }
        caps.smaa = m_smaaPossible;
        caps.maxAnisotropy = m_maxAnisotropy;
        return caps;
    }
    bool hasStencilBuffer() const override { return hasStencil(m_pp.AutoDepthStencilFormat); }
    uint32_t refreshRate() const override {
        D3DDISPLAYMODE mode{};
        return SUCCEEDED(m_device->GetDisplayMode(0, &mode)) ? mode.RefreshRate : 0;
    }
    void setBrightness(float brightness) override { m_brightnessLevel = brightness; }
    void setGammaRamp(uint32_t flags, const uint16_t* red, const uint16_t* green, const uint16_t* blue) override {
        D3DGAMMARAMP ramp{};
        for (size_t i = 0; i < 256; ++i) {
            ramp.red[i] = red[i];
            ramp.green[i] = green[i];
            ramp.blue[i] = blue[i];
        }
        m_device->SetGammaRamp(0, flags, &ramp);
    }

    TextureId createTexture(const TextureData& data) override {
        const auto use = useDevice();
        const Layout layout = layoutOf(data, 1);
        if (layout.levels == 0) {
            return 0;
        }
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(m_device->CreateTexture(data.width, data.height, layout.levels, 0, layout.format, D3DPOOL_MANAGED,
                                           &texture, nullptr))) {
            return 0;
        }
        for (uint32_t l = 0; l < layout.levels; ++l) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(texture->LockRect(l, &locked, nullptr, 0))) {
                copyLevel(layout, data, l, layout.starts[l], locked);
                texture->UnlockRect(l);
            }
        }
        m_textures.push_back(texture);
        return static_cast<TextureId>(m_textures.size());
    }

    TextureId createCubeTexture(const TextureData& data) override {
        const auto use = useDevice();
        const Layout layout = layoutOf(data, 6);
        if (layout.levels == 0) {
            return 0;
        }
        IDirect3DCubeTexture9* texture = nullptr;
        if (FAILED(m_device->CreateCubeTexture(data.width, layout.levels, 0, layout.format, D3DPOOL_MANAGED, &texture,
                                               nullptr))) {
            return 0;
        }
        for (uint32_t face = 0; face < 6; ++face) {
            for (uint32_t l = 0; l < layout.levels; ++l) {
                D3DLOCKED_RECT locked{};
                const auto f = static_cast<D3DCUBEMAP_FACES>(face);
                if (SUCCEEDED(texture->LockRect(f, l, &locked, nullptr, 0))) {
                    copyLevel(layout, data, l, face * layout.faceBytes + layout.starts[l], locked);
                    texture->UnlockRect(f, l);
                }
            }
        }
        m_textures.push_back(texture);
        return static_cast<TextureId>(m_textures.size());
    }

    void destroyTexture(TextureId id) override {
        const auto use = useDevice();
        if (id != 0 && id <= m_textures.size()) {
            release(m_textures[id - 1]);
            std::erase_if(m_targets, [id](const Target& t) { return t.id == id; });
            std::erase_if(m_dynamics, [id](const Dynamic& d) { return d.id == id; });
        }
    }

    TextureId createRenderTarget(uint32_t width, uint32_t height, bool cube) override {
        const auto use = useDevice();
        if (width == 0 || height == 0) return 0;
        Target t{0, width, height, cube};
        IDirect3DBaseTexture9* texture = makeTarget(t);
        if (texture == nullptr) return 0;
        m_textures.push_back(texture);
        t.id = static_cast<TextureId>(m_textures.size());
        m_targets.push_back(t);
        return t.id;
    }

    TextureId createFloatTarget(uint32_t width, uint32_t height) override {
        const auto use = useDevice();
        if (width == 0 || height == 0) return 0;
        Target t{0, width, height, false};
        t.floatColour = true;
        IDirect3DBaseTexture9* texture = makeTarget(t);
        if (texture == nullptr) return 0;
        m_textures.push_back(texture);
        t.id = static_cast<TextureId>(m_textures.size());
        m_targets.push_back(t);
        return t.id;
    }

    void setShadowMaps(const TextureId* maps, uint32_t count) override {
        for (uint32_t i = 0; i < count; ++i) {
            const TextureId id = maps[i];
            const DWORD sampler = kShadowSampler + i;
            m_device->SetTexture(sampler, id != 0 && id <= m_textures.size() ? m_textures[id - 1] : nullptr);
            m_device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            m_device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            m_device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            m_device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            m_device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        }
    }

    void setSunShadowMap(TextureId map) override {
        constexpr DWORD kSunSampler = 14;
        m_device->SetTexture(kSunSampler, map != 0 && map <= m_textures.size() ? m_textures[map - 1] : nullptr);
        m_device->SetSamplerState(kSunSampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        m_device->SetSamplerState(kSunSampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        m_device->SetSamplerState(kSunSampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        m_device->SetSamplerState(kSunSampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        m_device->SetSamplerState(kSunSampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }

    bool copyCubeFace(TextureId from, TextureId to, uint32_t face) override {
        const auto use = useDevice();
        const Target* a = from != 0 ? targetOf(from) : nullptr;
        const Target* b = to != 0 ? targetOf(to) : nullptr;
        if (a == nullptr || b == nullptr || !a->cube || !b->cube || m_textures[from - 1] == nullptr ||
            m_textures[to - 1] == nullptr) {
            return false;
        }
        const auto f = static_cast<D3DCUBEMAP_FACES>(face % 6);
        IDirect3DSurface9* source = nullptr;
        IDirect3DSurface9* target = nullptr;
        static_cast<IDirect3DCubeTexture9*>(m_textures[from - 1])->GetCubeMapSurface(f, 0, &source);
        static_cast<IDirect3DCubeTexture9*>(m_textures[to - 1])->GetCubeMapSurface(f, 0, &target);
        const D3DTEXTUREFILTERTYPE filter = a->width == b->width ? D3DTEXF_NONE : m_pictureFilter;
        const bool ok = source != nullptr && target != nullptr &&
                        SUCCEEDED(m_device->StretchRect(source, nullptr, target, nullptr, filter));
        release(source);
        release(target);
        return ok;
    }

    bool ambientOcclusion(TextureId depth, const AmbientOcclusionView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible || depth == 0 || depth > m_textures.size() || m_textures[depth - 1] == nullptr) {
            return false;
        }
        if (m_occlusion == nullptr) m_occlusion = std::make_unique<AmbientOcclusionPass>();
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(m_textures[depth - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&texture)))) {
            return false;
        }
        IDirect3DSurface9* target = nullptr;
        m_device->GetRenderTarget(0, &target);
        D3DVIEWPORT9 viewport{};
        m_device->GetViewport(&viewport);
        AmbientOcclusionPass::View view;
        view.tanX = v.tanX;
        view.tanY = v.tanY;
        view.range = v.range;
        view.radius = v.radius;
        view.strength = v.strength;
        view.bias = v.bias;
        view.fadeFrom = v.fadeFrom;
        view.fadeLength = v.fadeLength;
        view.foliageRadius = v.foliageRadius;
        view.foliageStrength = v.foliageStrength;
        view.foliageFadeFrom = v.foliageFadeFrom;
        view.foliageFadeLength = v.foliageFadeLength;
        const bool ok = target != nullptr && m_occlusion->Run(m_device, texture, target, viewport, view);
        if (ok && !m_occlusionLogged) {
            m_occlusionLogged = true;
            log::info("d3d9: ambient occlusion over {}x{}", viewport.Width, viewport.Height);
        }
        release(target);
        release(texture);
        return ok;
    }

    bool lightReflections(TextureId depth, const LightReflectionView& v) override {
        const auto use = useDevice();
        // The same device's needs as the ambient occlusion's (not a pure device, shaders 3.0).
        if (!m_occlusionPossible || depth == 0 || depth > m_textures.size() || m_textures[depth - 1] == nullptr) {
            return false;
        }
        if (m_reflections == nullptr) m_reflections = std::make_unique<LightReflectionPass>();
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(m_textures[depth - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&texture)))) {
            return false;
        }
        IDirect3DSurface9* target = nullptr;
        m_device->GetRenderTarget(0, &target);
        D3DVIEWPORT9 viewport{};
        m_device->GetViewport(&viewport);
        LightReflectionPass::View view;
        view.tanX = v.tanX;
        view.tanY = v.tanY;
        view.range = v.range;
        view.reach = v.reach;
        for (size_t i = 0; i < 3; ++i) view.up[i] = v.up[i];
        view.wetness = v.wetness;
        view.fromDry = v.fromDry;
        view.fromWet = v.fromWet;
        view.strengthDry = v.strengthDry;
        view.strengthWet = v.strengthWet;
        view.streak = v.streak;
        view.roughness = v.roughness;
        view.asphalt = v.asphalt;
        view.water = v.water;
        view.streakCount = std::clamp(v.streakCount, 0, LightReflectionPass::View::kStreaks);
        for (int k = 0; k < view.streakCount; ++k) {
            for (size_t c = 0; c < 8; ++c) view.streaks[k][c] = v.streaks[static_cast<size_t>(k)][c];
        }
        const bool ok = target != nullptr && m_reflections->Run(m_device, texture, target, viewport, view);
        if (ok && !m_reflectionsLogged) {
            m_reflectionsLogged = true;
            log::info("d3d9: lights reflected in the road over {}x{}", viewport.Width, viewport.Height);
        }
        release(target);
        release(texture);
        return ok;
    }

    bool lampShadows(TextureId depth, const std::array<TextureId, 4>& maps, const LampShadowView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible || depth == 0 || depth > m_textures.size() || m_textures[depth - 1] == nullptr ||
            v.count <= 0) {
            return false;
        }
        if (m_lampShadows == nullptr) m_lampShadows = std::make_unique<LampShadowPass>();
        const auto textureOf = [&](TextureId id) {
            IDirect3DTexture9* t = nullptr;
            if (id != 0 && id <= m_textures.size() && m_textures[id - 1] != nullptr) {
                m_textures[id - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&t));
            }
            return t;
        };
        IDirect3DTexture9* texture = textureOf(depth);
        IDirect3DTexture9* lampMaps[LampShadowPass::kLamps] = {};
        const int count = std::min(v.count, LampShadowPass::kLamps);
        for (int i = 0; i < count; ++i) lampMaps[i] = textureOf(maps[static_cast<size_t>(i)]);
        IDirect3DSurface9* target = nullptr;
        m_device->GetRenderTarget(0, &target);
        D3DVIEWPORT9 viewport{};
        m_device->GetViewport(&viewport);
        LampShadowPass::View view;
        view.tanX = v.tanX;
        view.tanY = v.tanY;
        view.range = v.range;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t k = 0; k < 4; ++k) view.camera[r][k] = v.camera[r][k];
        }
        view.strength = v.strength;
        view.bias = v.bias;
        view.texel = v.texel;
        view.count = count;
        for (int i = 0; i < count; ++i) {
            const LampShadowLamp& from = v.lamps[static_cast<size_t>(i)];
            LampShadowPass::Lamp& to = view.lamps[i];
            for (size_t r = 0; r < 3; ++r) {
                for (size_t k = 0; k < 4; ++k) to.clip[r][k] = from.clip[r][k];
            }
            for (size_t k = 0; k < 3; ++k) {
                to.place[k] = from.place[k];
                to.look[k] = from.look[k];
            }
            to.range = from.range;
            to.strength = from.strength;
        }
        const bool ok = texture != nullptr && target != nullptr &&
                        m_lampShadows->Run(m_device, texture, lampMaps, target, viewport, view);
        if (ok && !m_lampShadowsLogged) {
            m_lampShadowsLogged = true;
            log::info("d3d9: street lamps' shadows over {}x{} ({} lamps)", viewport.Width, viewport.Height, count);
        }
        for (IDirect3DTexture9*& t : lampMaps) release(t);
        release(target);
        release(texture);
        return ok;
    }

    bool captureAmbientOcclusion(const std::wstring& file) override {
        const auto use = useDevice();
        return captureTexture(m_occlusion != nullptr ? m_occlusion->Occlusion() : nullptr, file);
    }

    // A flat texture of an id, referenced; none for another kind or none.
    IDirect3DTexture9* flatTexture(TextureId id) {
        IDirect3DTexture9* t = nullptr;
        if (id != 0 && id <= m_textures.size() && m_textures[id - 1] != nullptr) {
            m_textures[id - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&t));
        }
        return t;
    }

    static CloudPass::View cloudPassView(const CloudView& v) {
        CloudPass::View view;
        view.height = v.height;
        view.fade = v.fade;
        view.threshold = CloudPass::ThresholdFor(v.cover);
        view.sharpness = v.sharpness;
        for (size_t r = 0; r < 2; ++r) {
            for (size_t k = 0; k < 4; ++k) view.project[r][k] = v.project[r][k];
        }
        for (size_t k = 0; k < 3; ++k) {
            view.eye[k] = v.eye[k];
            view.toSun[k] = v.toSun[k];
            view.sun[k] = v.sun[k];
            view.ambient[k] = v.ambient[k];
        }
        view.detailScale = v.detailScale;
        for (size_t k = 0; k < 4; ++k) view.wind[k] = v.wind[k];
        view.scatter = v.scatter;
        view.sunStep[0] = v.sunStep[0];
        view.sunStep[1] = v.sunStep[1];
        view.absorption = v.absorption;
        view.opacity = v.opacity;
        view.clearLight = v.clearLight;
        view.cloudLight = v.cloudLight;
        view.volume = v.volume ? 1.0f : 0.0f;
        view.top = v.top;
        view.extinction = v.extinction;
        view.steps = v.steps;
        view.lightSteps = v.lightSteps;
        view.farthest = v.farthest;
        view.weatherScale = v.weatherScale;
        view.shapeScale = v.shapeScale;
        view.erosionScale = v.erosionScale;
        view.overcast = v.overcast;
        view.screen = v.screen ? 1.0f : 0.0f;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t k = 0; k < 3; ++k) view.clip[r][k] = v.clip[r][k];
        }
        view.seconds = v.seconds;
        for (size_t k = 0; k < 3; ++k) {
            view.shapeDrift[k] = v.shapeDrift[k];
            view.erosionDrift[k] = v.erosionDrift[k];
        }
        return view;
    }

    bool cloudMap(TextureId map, const CloudView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible) return false;
        IDirect3DTexture9* texture = flatTexture(map);
        if (texture == nullptr) return false;
        if (m_clouds == nullptr) m_clouds = std::make_unique<CloudPass>();
        const bool ok = m_clouds->DrawMap(m_device, texture, cloudPassView(v));
        if (ok && !m_cloudsLogged) {
            m_cloudsLogged = true;
            log::info("d3d9: procedural clouds' map (noise {}x{} baked, cover {:.2f})", CloudPass::kNoiseSize,
                      CloudPass::kNoiseSize, v.cover);
        }
        release(texture);
        return ok;
    }

    bool cloudView(const CloudView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible) return false;
        if (m_clouds == nullptr) m_clouds = std::make_unique<CloudPass>();
        const bool ok = m_clouds->DrawView(m_device, cloudPassView(v));
        if (ok && !m_cloudViewLogged) {
            m_cloudViewLogged = true;
            D3DVIEWPORT9 viewport{};
            m_device->GetViewport(&viewport);
            log::info("d3d9: the volume clouds' view {}x{} at half size, blended over the frames", viewport.Width,
                      viewport.Height);
        }
        return ok;
    }

    bool cloudSky(uint32_t primitives, const CloudView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible) return false;
        if (m_clouds == nullptr) m_clouds = std::make_unique<CloudPass>();
        return m_clouds->DrawSky(m_device, primitives, cloudPassView(v));
    }

    bool dilateDepth(TextureId target, const Viewport& region, float gap) override {
        const auto use = useDevice();
        IDirect3DTexture9* texture = flatTexture(target);
        if (texture == nullptr) return false;
        if (m_dilate == nullptr) m_dilate = std::make_unique<DepthDilatePass>();
        const RECT rect{static_cast<LONG>(region.x), static_cast<LONG>(region.y),
                        static_cast<LONG>(region.x + region.width), static_cast<LONG>(region.y + region.height)};
        const bool ok = m_dilate->Run(m_device, texture, rect, gap);
        release(texture);
        return ok;
    }

    bool captureTarget(TextureId target, const std::wstring& file) override {
        const auto use = useDevice();
        IDirect3DTexture9* texture = flatTexture(target);
        const bool ok = captureTexture(texture, file);
        release(texture);
        return ok;
    }

    bool volumetricLight(TextureId depth, const VolumetricLightView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible || depth == 0 || depth > m_textures.size() || m_textures[depth - 1] == nullptr ||
            v.count <= 0) {
            return false;
        }
        if (m_volumetric == nullptr) m_volumetric = std::make_unique<VolumetricLightPass>();
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(m_textures[depth - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&texture)))) {
            return false;
        }
        IDirect3DSurface9* target = nullptr;
        m_device->GetRenderTarget(0, &target);
        D3DVIEWPORT9 viewport{};
        m_device->GetViewport(&viewport);
        VolumetricLightPass::View view;
        view.tanX = v.tanX;
        view.tanY = v.tanY;
        view.range = v.range;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t k = 0; k < 4; ++k) view.camera[r][k] = v.camera[r][k];
        }
        view.density = v.density;
        view.metre = v.metre;
        view.count = std::clamp(v.count, 0, VolumetricLightPass::kLights);
        for (int i = 0; i < view.count; ++i) {
            const VolumetricLight& from = v.lights[static_cast<size_t>(i)];
            VolumetricLightPass::Light& to = view.lights[i];
            for (size_t k = 0; k < 3; ++k) {
                to.place[k] = from.place[k];
                to.axis[k] = from.axis[k];
                to.side[k] = from.side[k];
                to.up[k] = from.up[k];
                to.colour[k] = from.colour[k];
            }
            to.reach = from.reach;
            to.steps = from.steps;
            to.falloff = from.falloff;
            to.forward = from.forward;
        }
        const bool ok = target != nullptr && m_volumetric->Run(m_device, texture, target, viewport, view);
        if (ok && !m_volumetricLogged) {
            m_volumetricLogged = true;
            log::info("d3d9: light in the haze over {}x{} ({} lights)", viewport.Width, viewport.Height, view.count);
        }
        release(target);
        release(texture);
        return ok;
    }

    bool pointLights(TextureId depth, const PointLightView& v) override {
        const auto use = useDevice();
        if (!m_occlusionPossible || depth == 0 || depth > m_textures.size() || m_textures[depth - 1] == nullptr ||
            v.count <= 0) {
            return false;
        }
        if (m_pointLights == nullptr) m_pointLights = std::make_unique<PointLightPass>();
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(m_textures[depth - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&texture)))) {
            return false;
        }
        IDirect3DSurface9* target = nullptr;
        m_device->GetRenderTarget(0, &target);
        D3DVIEWPORT9 viewport{};
        m_device->GetViewport(&viewport);
        PointLightPass::View view;
        view.tanX = v.tanX;
        view.tanY = v.tanY;
        view.range = v.range;
        for (size_t r = 0; r < 3; ++r) {
            for (size_t k = 0; k < 4; ++k) view.camera[r][k] = v.camera[r][k];
        }
        view.count = std::clamp(v.count, 0, PointLightPass::kLights);
        for (int i = 0; i < view.count; ++i) {
            const PointLight& from = v.lights[static_cast<size_t>(i)];
            PointLightPass::Light& to = view.lights[i];
            for (size_t k = 0; k < 3; ++k) {
                to.place[k] = from.place[k];
                to.colour[k] = from.colour[k];
            }
            to.reach = from.reach;
            to.falloff = from.falloff;
        }
        const bool ok = target != nullptr && m_pointLights->Run(m_device, texture, target, viewport, view);
        if (ok && !m_pointLightsLogged) {
            m_pointLightsLogged = true;
            log::info("d3d9: point lights over {}x{} ({} lights)", viewport.Width, viewport.Height, view.count);
        }
        release(target);
        release(texture);
        return ok;
    }

    bool capturePointLights(const std::wstring& file) override {
        const auto use = useDevice();
        return captureTexture(m_pointLights != nullptr ? m_pointLights->Lit() : nullptr, file);
    }

    bool captureVolumetricLight(const std::wstring& file) override {
        const auto use = useDevice();
        return captureTexture(m_volumetric != nullptr ? m_volumetric->Scattered() : nullptr, file);
    }

    bool captureLampShadows(const std::wstring& file) override {
        const auto use = useDevice();
        return captureTexture(m_lampShadows != nullptr ? m_lampShadows->Shading() : nullptr, file);
    }

    bool captureLightReflections(const std::wstring& file) override {
        const auto use = useDevice();
        return captureTexture(m_reflections != nullptr ? m_reflections->Reflection() : nullptr, file);
    }

    // A render target texture's first level as a BMP.
    bool captureTexture(IDirect3DTexture9* texture, const std::wstring& file) {
        if (texture == nullptr) return false;
        IDirect3DSurface9* surface = nullptr;
        IDirect3DSurface9* system = nullptr;
        D3DSURFACE_DESC desc{};
        bool ok = false;
        if (SUCCEEDED(texture->GetSurfaceLevel(0, &surface)) && SUCCEEDED(surface->GetDesc(&desc)) &&
            SUCCEEDED(m_device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                            &system, nullptr)) &&
            SUCCEEDED(m_device->GetRenderTargetData(surface, system))) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(system->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
                ok = writeBmp(file, desc.Width, desc.Height, static_cast<const uint8_t*>(locked.pBits), locked.Pitch,
                              desc.Format);
                system->UnlockRect();
            }
        }
        release(system);
        release(surface);
        return ok;
    }

    bool captureCubeFace(TextureId id, uint32_t face, const std::wstring& file) override {
        const auto use = useDevice();
        const Target* t = id != 0 ? targetOf(id) : nullptr;
        if (t == nullptr || !t->cube || m_textures[id - 1] == nullptr) return false;
        IDirect3DSurface9* surface = nullptr;
        IDirect3DSurface9* system = nullptr;
        D3DSURFACE_DESC desc{};
        bool ok = false;
        static_cast<IDirect3DCubeTexture9*>(m_textures[id - 1])
            ->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face % 6), 0, &surface);
        if (surface != nullptr && SUCCEEDED(surface->GetDesc(&desc)) &&
            SUCCEEDED(m_device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                            &system, nullptr)) &&
            SUCCEEDED(m_device->GetRenderTargetData(surface, system))) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(system->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
                ok = writeBmp(file, desc.Width, desc.Height, static_cast<const uint8_t*>(locked.pBits), locked.Pitch,
                              desc.Format);
                system->UnlockRect();
            }
        }
        release(system);
        release(surface);
        return ok;
    }

    bool captureFloatTarget(TextureId id, const std::wstring& file) override {
        const auto use = useDevice();
        if (id == 0 || id > m_textures.size() || m_textures[id - 1] == nullptr) return false;
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(m_textures[id - 1]->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&texture)))) {
            return false;
        }
        IDirect3DSurface9* surface = nullptr;
        IDirect3DSurface9* system = nullptr;
        D3DSURFACE_DESC desc{};
        bool ok = false;
        if (SUCCEEDED(texture->GetSurfaceLevel(0, &surface)) && SUCCEEDED(surface->GetDesc(&desc)) &&
            desc.Format == D3DFMT_R32F &&
            SUCCEEDED(m_device->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_R32F, D3DPOOL_SYSTEMMEM,
                                                            &system, nullptr)) &&
            SUCCEEDED(m_device->GetRenderTargetData(surface, system))) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(system->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
                std::vector<uint8_t> grey(size_t(desc.Width) * desc.Height * 4);
                for (uint32_t y = 0; y < desc.Height; ++y) {
                    const float* row = reinterpret_cast<const float*>(static_cast<const uint8_t*>(locked.pBits) +
                                                                      size_t(y) * locked.Pitch);
                    for (uint32_t x = 0; x < desc.Width; ++x) {
                        const float v = std::clamp(row[x], 0.0f, 1.0f);
                        const auto g = static_cast<uint8_t>(v * 255.0f + 0.5f);
                        uint8_t* p = grey.data() + (size_t(y) * desc.Width + x) * 4;
                        p[0] = p[1] = p[2] = g;
                        p[3] = 255;
                    }
                }
                system->UnlockRect();
                ok = writeBmp(file, desc.Width, desc.Height, grey.data(), static_cast<int>(desc.Width * 4),
                              D3DFMT_X8R8G8B8);
            }
        }
        release(system);
        release(surface);
        release(texture);
        return ok;
    }

    void multisampleTarget(TextureId id) override {
        for (Target& t : m_targets) {
            if (t.id == id && !t.cube) t.multisampled = true;
        }
    }

    void setRenderTarget(TextureId id, uint32_t face) override {
        m_viewportSet = false;  // a render target set: the viewport its whole
        resolveSamples();
        if (id == 0) {
            if (m_backColor == nullptr) {
                m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_backColor);
                m_device->GetDepthStencilSurface(&m_backDepth);
            }
            if (m_backColor != nullptr) m_device->SetRenderTarget(0, m_backColor);
            m_device->SetDepthStencilSurface(m_backDepth);
            m_target = 0;
            return;
        }
        const Target* t = targetOf(id);
        if (t == nullptr || id > m_textures.size() || m_textures[id - 1] == nullptr) return;
        if (m_backColor == nullptr) {
            m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_backColor);
            m_device->GetDepthStencilSurface(&m_backDepth);
        }
        if (const Samples* samples = t->multisampled ? samplesOf(*t) : nullptr) {
            m_device->SetRenderTarget(0, samples->colour);
            m_device->SetDepthStencilSurface(samples->depth);
            m_target = id;
            m_samplesBound = id;
            return;
        }
        IDirect3DSurface9* surface = nullptr;
        if (t->cube) {
            static_cast<IDirect3DCubeTexture9*>(m_textures[id - 1])
                ->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face % 6), 0, &surface);
        } else {
            static_cast<IDirect3DTexture9*>(m_textures[id - 1])->GetSurfaceLevel(0, &surface);
        }
        if (surface == nullptr) return;
        m_device->SetRenderTarget(0, surface);
        surface->Release();
        m_device->SetDepthStencilSurface(depthOf(t->width, t->height));
        m_target = id;
    }

    uint32_t targetWidth(TextureId id) const override {
        const Target* t = id != 0 ? targetOf(id) : nullptr;
        return t != nullptr ? t->width : screenWidth();
    }
    uint32_t targetHeight(TextureId id) const override {
        const Target* t = id != 0 ? targetOf(id) : nullptr;
        return t != nullptr ? t->height : screenHeight();
    }

    uint32_t resets() const override { return m_resets; }

    bool readTarget(TextureId id, std::vector<uint8_t>& pixels, uint32_t* pitch) override {
        if (id != 0 && id == m_samplesBound) resolveSamples();
        const Target* t = id != 0 ? targetOf(id) : nullptr;
        if (t == nullptr || t->cube || id > m_textures.size() || m_textures[id - 1] == nullptr) return false;
        IDirect3DSurface9* surface = nullptr;
        static_cast<IDirect3DTexture9*>(m_textures[id - 1])->GetSurfaceLevel(0, &surface);
        if (surface == nullptr) return false;
        D3DSURFACE_DESC desc{};
        surface->GetDesc(&desc);
        IDirect3DSurface9* copy = nullptr;
        bool read = false;
        if (SUCCEEDED(m_device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                            &copy, nullptr)) &&
            SUCCEEDED(m_device->GetRenderTargetData(surface, copy))) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
                const auto bytes = static_cast<size_t>(locked.Pitch) * desc.Height;
                const auto* from = static_cast<const uint8_t*>(locked.pBits);
                pixels.assign(from, from + bytes);
                *pitch = static_cast<uint32_t>(locked.Pitch);
                copy->UnlockRect();
                read = true;
            }
        }
        release(copy);
        surface->Release();
        return read;
    }

    uint32_t displayFormat() const override {
        D3DDISPLAYMODE mode{};
        return SUCCEEDED(m_d3d->GetAdapterDisplayMode(m_adapterIndex, &mode)) ? mode.Format : 0;
    }
    uint32_t textureCaps() const override { return m_textureCaps; }
    bool textureFormatOk(uint32_t display, uint32_t format) const override {
        return SUCCEEDED(m_d3d->CheckDeviceFormat(m_adapterIndex, D3DDEVTYPE_HAL, static_cast<D3DFORMAT>(display),
                                                  0, D3DRTYPE_TEXTURE, static_cast<D3DFORMAT>(format)));
    }

    TextureId createDynamicTexture(uint32_t width, uint32_t height, uint32_t format) override {
        const auto use = useDevice();
        Dynamic d{0, width, height, format, true};
        IDirect3DTexture9* texture = makeDynamic(d);
        if (texture == nullptr) return 0;
        m_textures.push_back(texture);
        d.id = static_cast<TextureId>(m_textures.size());
        m_dynamics.push_back(d);
        return d.id;
    }
    bool lockTexture(TextureId id, void** bits, int32_t* pitch) override {
        const auto use = useDevice();
        if (id == 0 || id > m_textures.size() || m_textures[id - 1] == nullptr) return false;
        D3DLOCKED_RECT locked{};
        if (FAILED(static_cast<IDirect3DTexture9*>(m_textures[id - 1])->LockRect(0, &locked, nullptr, 0))) {
            return false;
        }
        *bits = locked.pBits;
        *pitch = locked.Pitch;
        return true;
    }
    void unlockTexture(TextureId id) override {
        const auto use = useDevice();
        if (id != 0 && id <= m_textures.size() && m_textures[id - 1] != nullptr) {
            static_cast<IDirect3DTexture9*>(m_textures[id - 1])->UnlockRect(0);
        }
    }
    void drawUp(uint32_t primitive, uint32_t primitiveCount, const void* vertices, uint32_t stride) override {
        m_device->DrawPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(primitive), primitiveCount, vertices, stride);
        m_profiler.count(primitiveCount);
    }

    void drawQuad(const QuadVertex (&c)[4], TextureId texture, Blend blend, bool samplerSet) override {
        struct Vertex {
            float x, y, z, rhw;
            D3DCOLOR color;
            float u, v;
        };
        const Vertex v[4] = {
            {c[0].x, c[0].y, 0.0f, 1.0f, c[0].argb, c[0].u, c[0].v},
            {c[1].x, c[1].y, 0.0f, 1.0f, c[1].argb, c[1].u, c[1].v},
            {c[2].x, c[2].y, 0.0f, 1.0f, c[2].argb, c[2].u, c[2].v},
            {c[3].x, c[3].y, 0.0f, 1.0f, c[3].argb, c[3].u, c[3].v},
        };
        IDirect3DBaseTexture9* t = (texture != 0 && texture <= m_textures.size()) ? m_textures[texture - 1] : nullptr;
        m_device->SetVertexShader(nullptr);
        m_device->SetPixelShader(nullptr);
        m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        m_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_device->SetRenderState(D3DRS_DESTBLEND, blend == Blend::Additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
        m_device->SetTexture(0, t);
        if (t != nullptr && !samplerSet) {
            m_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            m_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            m_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            m_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            m_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        }
        m_device->SetTextureStageState(0, D3DTSS_COLOROP, t != nullptr ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
        m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, t != nullptr ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        m_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
    }

    void drawLine(float x0, float y0, float x1, float y1, uint32_t argb) override {
        struct Vertex {
            float x, y, z, rhw;
            D3DCOLOR color;
        };
        const Vertex v[2] = {{x0, y0, 0.0f, 1.0f, argb}, {x1, y1, 0.0f, 1.0f, argb}};
        m_device->SetVertexShader(nullptr);
        m_device->SetPixelShader(nullptr);
        m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        m_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        m_device->SetTexture(0, nullptr);
        m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        m_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        m_device->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(Vertex));
    }

    void setClip(const ClipRect* rect) override {
        m_scissor = rect != nullptr;
        if (rect == nullptr) {
            m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            return;
        }
        RECT r{};
        r.left = std::max(0L, static_cast<LONG>(rect->x0));
        r.top = std::max(0L, static_cast<LONG>(rect->y0));
        r.right = std::min(static_cast<LONG>(screenWidth()), static_cast<LONG>(rect->x1));
        r.bottom = std::min(static_cast<LONG>(screenHeight()), static_cast<LONG>(rect->y1));
        m_device->SetScissorRect(&r);
        m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    }

    bool capture(const std::wstring& file) override {
        const auto use = useDevice();
        IDirect3DSurface9* back = nullptr;
        if (m_frameColor != nullptr) {
            back = m_frameColor;  // the picture, without the bars around it
            back->AddRef();
        } else if (FAILED(m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) {
            return false;
        }
        D3DSURFACE_DESC desc{};
        back->GetDesc(&desc);
        IDirect3DSurface9* copy = nullptr;
        IDirect3DSurface9* source = back;
        bool ok = false;
        if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            // A multisampled buffer is resolved into a plain render target first.
            if (SUCCEEDED(m_device->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0,
                                                       FALSE, &copy, nullptr))) {
                m_device->StretchRect(back, nullptr, copy, nullptr, D3DTEXF_NONE);
                source = copy;
            }
        }
        IDirect3DSurface9* system = nullptr;
        if (SUCCEEDED(m_device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                            &system, nullptr)) &&
            SUCCEEDED(m_device->GetRenderTargetData(source, system))) {
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(system->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
                const bool jpeg = file.size() > 4 && _wcsicmp(file.c_str() + file.size() - 4, L".jpg") == 0;
                ok = jpeg ? writeJpeg(file, desc.Width, desc.Height, static_cast<const uint8_t*>(locked.pBits),
                                      locked.Pitch, desc.Format)
                          : writeBmp(file, desc.Width, desc.Height, static_cast<const uint8_t*>(locked.pBits),
                                     locked.Pitch, desc.Format);
                system->UnlockRect();
            }
        }
        release(system);
        release(copy);
        release(back);
        return ok;
    }

    ShaderId createVertexShader(const uint32_t* code) override {
        const auto use = useDevice();
        IDirect3DVertexShader9* shader = nullptr;
        // The fixed build's replacement of a lit material's shader (Render/ShaderReplacements), the game's when it
        // cannot be made.
        const char* name = nullptr;
        const std::vector<uint32_t> replaced = replaceShader(code, true, m_replacements, &name);
        if (!replaced.empty() &&
            SUCCEEDED(m_device->CreateVertexShader(reinterpret_cast<const DWORD*>(replaced.data()), &shader))) {
            noteReplaced(name);
        } else if (FAILED(m_device->CreateVertexShader(reinterpret_cast<const DWORD*>(code), &shader))) {
            return 0;
        }
        m_shaders.push_back(shader);
        return static_cast<ShaderId>(m_shaders.size());
    }

    ShaderId createPixelShader(const uint32_t* code) override {
        const auto use = useDevice();
        IDirect3DPixelShader9* shader = nullptr;
        const char* name = nullptr;
        const std::vector<uint32_t> replaced = replaceShader(code, false, m_replacements, &name, kBaseLightCase);
        if (!replaced.empty() &&
            SUCCEEDED(m_device->CreatePixelShader(reinterpret_cast<const DWORD*>(replaced.data()), &shader))) {
            noteReplaced(name);
        } else if (FAILED(m_device->CreatePixelShader(reinterpret_cast<const DWORD*>(code), &shader))) {
            return 0;
        }
        m_shaders.push_back(shader);
        const auto id = static_cast<ShaderId>(m_shaders.size());
        // Its variants of the other light cases (Render/LightCase), each made once, bound in its place in their
        // cases.
        std::array<ShaderId, kLightCases.size()> variants{};
        std::vector<std::pair<std::vector<uint32_t>, ShaderId>> made;
        for (size_t i = 0; i < kLightCases.size(); ++i) {
            if (kLightCases[i] == kBaseLightCase) continue;
            const char* caseName = nullptr;
            std::vector<uint32_t> variant = replaceShader(code, false, m_replacements, &caseName, kLightCases[i]);
            if (variant.empty() || variant == replaced) continue;
            const auto same = std::find_if(made.begin(), made.end(), [&](const auto& m) { return m.first == variant; });
            if (same != made.end()) {
                variants[i] = same->second;
                continue;
            }
            IDirect3DPixelShader9* other = nullptr;
            if (FAILED(m_device->CreatePixelShader(reinterpret_cast<const DWORD*>(variant.data()), &other))) continue;
            noteReplaced(caseName);
            m_shaders.push_back(other);
            variants[i] = static_cast<ShaderId>(m_shaders.size());
            made.emplace_back(std::move(variant), variants[i]);
        }
        if (!made.empty()) m_caseVariants[id] = variants;
        return id;
    }

    DeclarationId createDeclaration(const VertexElement* elements) override {
        const auto use = useDevice();
        static_assert(sizeof(VertexElement) == sizeof(D3DVERTEXELEMENT9));
        IDirect3DVertexDeclaration9* declaration = nullptr;
        if (FAILED(m_device->CreateVertexDeclaration(reinterpret_cast<const D3DVERTEXELEMENT9*>(elements),
                                                     &declaration))) {
            return 0;
        }
        m_declarations.push_back(declaration);
        return static_cast<DeclarationId>(m_declarations.size());
    }

    BufferId createVertexBuffer(const void* data, uint32_t bytes) override {
        const auto use = useDevice();
        IDirect3DVertexBuffer9* buffer = nullptr;
        if (bytes == 0 || FAILED(m_device->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &buffer,
                                                               nullptr))) {
            return 0;
        }
        void* p = nullptr;
        if (SUCCEEDED(buffer->Lock(0, bytes, &p, 0))) {
            std::memcpy(p, data, bytes);
            buffer->Unlock();
        }
        m_buffers.push_back(buffer);
        return static_cast<BufferId>(m_buffers.size());
    }

    BufferId createIndexBuffer(const uint16_t* indices, uint32_t count) override {
        const auto use = useDevice();
        IDirect3DIndexBuffer9* buffer = nullptr;
        const UINT bytes = count * 2;
        if (count == 0 || FAILED(m_device->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                                              D3DPOOL_MANAGED, &buffer, nullptr))) {
            return 0;
        }
        void* p = nullptr;
        if (SUCCEEDED(buffer->Lock(0, bytes, &p, 0))) {
            std::memcpy(p, indices, bytes);
            buffer->Unlock();
        }
        m_buffers.push_back(buffer);
        return static_cast<BufferId>(m_buffers.size());
    }

    void updateVertexBuffer(BufferId id, const void* data, uint32_t bytes) override {
        const auto use = useDevice();
        auto* buffer = static_cast<IDirect3DVertexBuffer9*>(bufferAt(id));
        void* p = nullptr;
        if (buffer != nullptr && bytes != 0 && SUCCEEDED(buffer->Lock(0, bytes, &p, 0))) {
            std::memcpy(p, data, bytes);
            buffer->Unlock();
        }
    }

    bool occlusionQueries() const override {
        return m_device != nullptr && SUCCEEDED(m_device->CreateQuery(D3DQUERYTYPE_OCCLUSION, nullptr));
    }
    QueryId createOcclusionQuery() override {
        const auto use = useDevice();
        IDirect3DQuery9* query = nullptr;
        if (FAILED(m_device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query))) return 0;
        m_queries.push_back(query);
        return static_cast<QueryId>(m_queries.size());
    }
    void issueQuery(QueryId id, bool begin) override {
        if (id != 0 && id <= m_queries.size() && m_queries[id - 1] != nullptr) {
            m_queries[id - 1]->Issue(begin ? D3DISSUE_BEGIN : D3DISSUE_END);
        }
    }
    int queryResult(QueryId id, bool flush, uint32_t* pixels) override {
        if (id == 0 || id > m_queries.size() || m_queries[id - 1] == nullptr) return -1;
        DWORD count = 0;
        const HRESULT hr = m_queries[id - 1]->GetData(&count, sizeof(count), flush ? D3DGETDATA_FLUSH : 0);
        if (hr == S_OK) {
            *pixels = count;
            return 0;
        }
        return hr == S_FALSE ? 1 : -1;
    }

    void destroyBuffer(BufferId id) override {
        const auto use = useDevice();
        if (id != 0 && id <= m_buffers.size()) {
            release(m_buffers[id - 1]);
        }
    }

    void setRenderState(RenderState state, uint32_t value) override {
        m_device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state), value);
    }
    void setTextureStageState(uint32_t stage, TextureStageState type, uint32_t value) override {
        m_device->SetTextureStageState(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(type), value);
    }
    void setSamplerState(uint32_t sampler, SamplerState type, uint32_t value) override {
        // The fixed build's texture filter and LOD bias over the game's states (Render/FrameOptions), the game's kept.
        const auto k = std::find(kQualityStates.begin(), kQualityStates.end(), type);
        if (sampler < kSamplers && k != kQualityStates.end()) {
            m_gameSamplers[sampler][static_cast<size_t>(k - kQualityStates.begin())] = value;
        }
        m_device->SetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(type), m_quality.adjust(type, value));
    }
    void setTexture(uint32_t sampler, TextureId texture) override {
        IDirect3DBaseTexture9* t = (texture != 0 && texture <= m_textures.size()) ? m_textures[texture - 1] : nullptr;
        m_device->SetTexture(sampler, t);
    }
    void setTransform(TransformState state, const float* matrix) override {
        m_device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(state), reinterpret_cast<const D3DMATRIX*>(matrix));
    }
    void setMaterial(const FixedMaterial& material) override {
        static_assert(sizeof(FixedMaterial) == sizeof(D3DMATERIAL9));
        m_device->SetMaterial(reinterpret_cast<const D3DMATERIAL9*>(&material));
    }
    void setLight(uint32_t index, const FixedLight& light) override {
        static_assert(sizeof(FixedLight) == sizeof(D3DLIGHT9));
        m_device->SetLight(index, reinterpret_cast<const D3DLIGHT9*>(&light));
    }
    void lightEnable(uint32_t index, bool enable) override { m_device->LightEnable(index, enable ? TRUE : FALSE); }
    void setFvf(uint32_t fvf) override { m_device->SetFVF(fvf); }
    void setNPatchMode(float segments) override { m_device->SetNPatchMode(segments); }
    void setVertexShader(ShaderId shader) override {
        m_device->SetVertexShader(static_cast<IDirect3DVertexShader9*>(shaderAt(shader)));
    }
    void setPixelShader(ShaderId shader) override {
        if (const auto it = m_caseVariants.find(shader); it != m_caseVariants.end() && it->second[m_case] != 0) {
            shader = it->second[m_case];
        }
        m_device->SetPixelShader(static_cast<IDirect3DPixelShader9*>(shaderAt(shader)));
    }
    void destroyShader(ShaderId shader) override {
        const auto use = useDevice();
        if (shader != 0 && shader <= m_shaders.size()) release(m_shaders[shader - 1]);
        if (const auto it = m_caseVariants.find(shader); it != m_caseVariants.end()) {
            for (const ShaderId variant : it->second) {
                if (variant != 0 && variant <= m_shaders.size()) release(m_shaders[variant - 1]);
            }
            m_caseVariants.erase(it);
        }
    }
    void setLightCase(LightCase lightCase) override { m_case = lightCaseIndex(lightCase); }
    void setShaderFeatures(unsigned features, float worldLightStrength) override {
        m_replacements.features = features;
        m_replacements.worldLightStrength = worldLightStrength;
    }
    void setHeatHazeStrength(float strength) override {
        if (m_heatHaze != nullptr) m_heatHaze->setStrength(strength);
    }
    void setVertexShaderConstantF(uint32_t start, const float* data, uint32_t count) override {
        m_device->SetVertexShaderConstantF(start, data, count);
    }
    void setVertexShaderConstantI(uint32_t start, const int32_t* data, uint32_t count) override {
        m_device->SetVertexShaderConstantI(start, reinterpret_cast<const int*>(data), count);
    }
    void setVertexShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) override {
        m_device->SetVertexShaderConstantB(start, reinterpret_cast<const BOOL*>(data), count);
    }
    void setPixelShaderConstantF(uint32_t start, const float* data, uint32_t count) override {
        m_device->SetPixelShaderConstantF(start, data, count);
    }
    void setPixelShaderConstantI(uint32_t start, const int32_t* data, uint32_t count) override {
        m_device->SetPixelShaderConstantI(start, reinterpret_cast<const int*>(data), count);
    }
    void setPixelShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) override {
        m_device->SetPixelShaderConstantB(start, reinterpret_cast<const BOOL*>(data), count);
    }
    void setDeclaration(DeclarationId declaration) override {
        m_declaration = declaration;
        m_device->SetVertexDeclaration(declaration != 0 && declaration <= m_declarations.size()
                                           ? m_declarations[declaration - 1]
                                           : nullptr);
    }
    void setStream(BufferId buffer, uint32_t stride) override {
        m_device->SetStreamSource(0, static_cast<IDirect3DVertexBuffer9*>(bufferAt(buffer)), 0, stride);
    }
    void setIndices(BufferId buffer) override {
        m_device->SetIndices(static_cast<IDirect3DIndexBuffer9*>(bufferAt(buffer)));
    }
    void drawIndexed(uint32_t primitive, uint32_t vertexCount, uint32_t startIndex, uint32_t primitiveCount) override {
        m_device->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive), 0, 0, vertexCount, startIndex,
                                       primitiveCount);
        m_profiler.count(primitiveCount);
    }
    void draw(uint32_t primitive, uint32_t startVertex, uint32_t primitiveCount) override {
        m_device->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive), startVertex, primitiveCount);
        m_profiler.count(primitiveCount);
    }
    void setViewport(const Viewport& v) override {
        const D3DVIEWPORT9 vp{v.x, v.y, v.width, v.height, v.minZ, v.maxZ};
        m_device->SetViewport(&vp);
        m_viewport = vp;
        m_viewportSet = true;
    }
    void clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil) override {
        if (!hasStencil(m_pp.AutoDepthStencilFormat)) {
            flags &= ~static_cast<uint32_t>(D3DCLEAR_STENCIL);
        }
        m_device->Clear(0, nullptr, flags, argb, z, stencil);
    }

private:
    bool onDeviceThread() const { return std::this_thread::get_id() == m_thread; }
    // The device held for a call; another thread's call counted while it waits.
    std::unique_lock<std::recursive_mutex> useDevice() {
        if (onDeviceThread()) return std::unique_lock(m_deviceLock);
        ++m_waiting;
        std::unique_lock lock(m_deviceLock);
        --m_waiting;
        return lock;
    }

    IUnknown* shaderAt(ShaderId id) const { return id != 0 && id <= m_shaders.size() ? m_shaders[id - 1] : nullptr; }
    IDirect3DResource9* bufferAt(BufferId id) const {
        return id != 0 && id <= m_buffers.size() ? m_buffers[id - 1] : nullptr;
    }

    // 32-bit X8R8G8B8 / A8R8G8B8 rows to a bottom-up 24-bit BMP.
    static bool writeBmp(const std::wstring& file, uint32_t w, uint32_t h, const uint8_t* bits, int pitch,
                         D3DFORMAT format) {
        if (format != D3DFMT_X8R8G8B8 && format != D3DFMT_A8R8G8B8) {
            return false;
        }
        const uint32_t row = (w * 3 + 3) & ~3u;
        BITMAPFILEHEADER fh{};
        BITMAPINFOHEADER ih{};
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + row * h;
        ih.biSize = sizeof(ih);
        ih.biWidth = static_cast<LONG>(w);
        ih.biHeight = static_cast<LONG>(h);
        ih.biPlanes = 1;
        ih.biBitCount = 24;
        std::vector<uint8_t> out(size_t(row) * h);
        for (uint32_t y = 0; y < h; ++y) {
            const uint8_t* src = bits + size_t(h - 1 - y) * pitch;
            uint8_t* dst = out.data() + size_t(y) * row;
            for (uint32_t x = 0; x < w; ++x) {
                dst[x * 3 + 0] = src[x * 4 + 0];
                dst[x * 3 + 1] = src[x * 4 + 1];
                dst[x * 3 + 2] = src[x * 4 + 2];
            }
        }
        HANDLE f = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD written = 0;
        WriteFile(f, &fh, sizeof(fh), &written, nullptr);
        WriteFile(f, &ih, sizeof(ih), &written, nullptr);
        WriteFile(f, out.data(), static_cast<DWORD>(out.size()), &written, nullptr);
        CloseHandle(f);
        return true;
    }

    // A JPEG through the Windows Imaging Component (the stock game's D3DXSaveSurfaceToFile is not available): 24-bit
    // rows from the 32-bit buffer, the encoder's default quality.
    static bool writeJpeg(const std::wstring& file, uint32_t w, uint32_t h, const uint8_t* bits, int pitch,
                          D3DFORMAT format) {
        if (format != D3DFMT_X8R8G8B8 && format != D3DFMT_A8R8G8B8) return false;
        const uint32_t row = w * 3;
        std::vector<uint8_t> rgb(size_t(row) * h);
        for (uint32_t y = 0; y < h; ++y) {
            const uint8_t* src = bits + size_t(y) * pitch;
            uint8_t* dst = rgb.data() + size_t(y) * row;
            for (uint32_t x = 0; x < w; ++x) {
                dst[x * 3 + 0] = src[x * 4 + 0];
                dst[x * 3 + 1] = src[x * 4 + 1];
                dst[x * 3 + 2] = src[x * 4 + 2];
            }
        }
        const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool ok = false;
        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        IPropertyBag2* options = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, &options)) && SUCCEEDED(frame->Initialize(options)) &&
            SUCCEEDED(frame->SetSize(w, h))) {
            WICPixelFormatGUID pixels = GUID_WICPixelFormat24bppBGR;
            ok = SUCCEEDED(frame->SetPixelFormat(&pixels)) && pixels == GUID_WICPixelFormat24bppBGR &&
                 SUCCEEDED(frame->WritePixels(h, row, static_cast<UINT>(rgb.size()), rgb.data())) &&
                 SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
        }
        release(options);
        release(frame);
        release(encoder);
        release(stream);
        release(factory);
        if (SUCCEEDED(init)) CoUninitialize();
        return ok;
    }

    // Back buffer: the display mode's format when windowed, else the first accepted of the list; depth: the first
    // accepted of the list; multisample: NONMASKABLE with min(levels, antialiasing) - 1.
    bool chooseFormats(const DeviceDesc& desc, std::string* error) {
        if (desc.depthBits != 16 && desc.depthBits != 32) {
            *error = std::format("RezDepth {} is neither 16 nor 32", desc.depthBits);
            return false;
        }
        D3DDISPLAYMODE mode{};
        const bool haveMode = SUCCEEDED(m_d3d->GetAdapterDisplayMode(m_adapterIndex, &mode));
        const D3DFORMAT display = haveMode ? mode.Format : D3DFMT_R5G6B5;
        const bool deep = desc.depthBits == 32;
        const D3DFORMAT* colours = deep ? kBackBuffer32 : kBackBuffer16;
        const size_t colourCount = deep ? std::size(kBackBuffer32) : std::size(kBackBuffer16);
        const D3DFORMAT* depths = deep ? kDepth32 : kDepth16;
        const size_t depthCount = deep ? std::size(kDepth32) : std::size(kDepth16);
        for (size_t d = 0; d < depthCount; ++d) {
            for (size_t c = 0; c < colourCount; ++c) {
                const D3DFORMAT colour = desc.windowed ? display : colours[c];
                const D3DFORMAT adapterFormat = desc.windowed ? display : colour;
                if (FAILED(m_d3d->CheckDeviceType(m_adapterIndex, D3DDEVTYPE_HAL, adapterFormat, colour,
                                                  desc.windowed)) ||
                    FAILED(m_d3d->CheckDeviceFormat(m_adapterIndex, D3DDEVTYPE_HAL, adapterFormat,
                                                    D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depths[d])) ||
                    FAILED(m_d3d->CheckDepthStencilMatch(m_adapterIndex, D3DDEVTYPE_HAL, adapterFormat, colour,
                                                         depths[d]))) {
                    continue;
                }
                m_pp.BackBufferFormat = colour;
                m_pp.AutoDepthStencilFormat = depths[d];
                // The fixed build's MSAA: the most samples up to the setting's (16 at most) both the colour and the
                // depth format take, at their highest quality; in place of the stock's antialiasing.
                if (desc.msaa >= 2) {
                    std::tie(m_pp.MultiSampleType, m_pp.MultiSampleQuality) = msaaSamples(desc.msaa);
                    return true;
                }
                DWORD colourLevels = 0;
                DWORD depthLevels = 0;
                if (desc.antialiasing != 0 &&
                    SUCCEEDED(m_d3d->CheckDeviceMultiSampleType(m_adapterIndex, D3DDEVTYPE_HAL, colour,
                                                                desc.windowed, D3DMULTISAMPLE_NONMASKABLE,
                                                                &colourLevels)) &&
                    SUCCEEDED(m_d3d->CheckDeviceMultiSampleType(m_adapterIndex, D3DDEVTYPE_HAL, depths[d],
                                                                desc.windowed, D3DMULTISAMPLE_NONMASKABLE,
                                                                &depthLevels)) &&
                    colourLevels != 0) {
                    const DWORD levels = std::min<DWORD>(colourLevels, desc.antialiasing);
                    m_pp.MultiSampleType = D3DMULTISAMPLE_NONMASKABLE;
                    m_pp.MultiSampleQuality = levels - 1;
                }
                return true;
            }
        }
        *error = "no back buffer and depth format the adapter accepts";
        return false;
    }

    // A lost device is reset once the driver allows it; a new window size resets it the same way.
    struct Target {
        TextureId id;
        uint32_t width, height;
        bool cube;
        bool multisampled = false;  // multisampleTarget
        bool floatColour = false;   // createFloatTarget: D3DFMT_R32F
    };

    // A multisampled target's colour and depth (multisampleTarget), one of a size: a frame's passes draw over one scene
    // target at a time and clear it first.
    struct Samples {
        IDirect3DSurface9* colour = nullptr;
        IDirect3DSurface9* depth = nullptr;
        bool failed = false;
    };

    // The frame's samples: the picture's surface's, else the back buffer's.
    D3DMULTISAMPLE_TYPE frameSamples() const { return m_frameWidth != 0 ? m_frameSamples : m_pp.MultiSampleType; }
    DWORD frameQuality() const { return m_frameWidth != 0 ? m_frameQuality : m_pp.MultiSampleQuality; }

    // A multisampled target's samples, made the first time; nullptr without MSAA or when they cannot be made.
    const Samples* samplesOf(const Target& t) {
        const D3DMULTISAMPLE_TYPE type = frameSamples();
        if (type == D3DMULTISAMPLE_NONE || m_textures[t.id - 1] == nullptr) return nullptr;
        Samples& s = m_samples[(static_cast<uint64_t>(t.width) << 32) | t.height];
        if (s.colour == nullptr && !s.failed) {
            D3DSURFACE_DESC desc{};
            static_cast<IDirect3DTexture9*>(m_textures[t.id - 1])->GetLevelDesc(0, &desc);
            if (FAILED(m_device->CreateRenderTarget(t.width, t.height, desc.Format, type, frameQuality(), FALSE,
                                                    &s.colour, nullptr)) ||
                FAILED(m_device->CreateDepthStencilSurface(t.width, t.height, m_pp.AutoDepthStencilFormat, type,
                                                           frameQuality(), TRUE, &s.depth, nullptr))) {
                release(s.colour);
                release(s.depth);
                s.failed = true;
                log::warning("d3d9: no multisampled target of {}x{}, the effects' frames without MSAA", t.width,
                             t.height);
            }
        }
        return s.colour != nullptr ? &s : nullptr;
    }

    // The bound multisampled target's samples resolved into its texture.
    void resolveSamples() {
        const TextureId id = m_samplesBound;
        m_samplesBound = 0;
        const Target* t = id != 0 ? targetOf(id) : nullptr;
        if (t == nullptr || m_textures[id - 1] == nullptr) return;
        const auto it = m_samples.find((static_cast<uint64_t>(t->width) << 32) | t->height);
        if (it == m_samples.end() || it->second.colour == nullptr) return;
        IDirect3DSurface9* surface = nullptr;
        static_cast<IDirect3DTexture9*>(m_textures[id - 1])->GetSurfaceLevel(0, &surface);
        if (surface == nullptr) return;
        m_device->StretchRect(it->second.colour, nullptr, surface, nullptr, D3DTEXF_NONE);
        surface->Release();
    }

    void releaseSamples() {
        for (auto& [key, s] : m_samples) {
            release(s.colour);
            release(s.depth);
        }
        m_samples.clear();
        m_samplesBound = 0;
    }

    // A texture the code writes: in the default pool (lost with the device) or the managed one.
    struct Dynamic {
        TextureId id;
        uint32_t width, height, format;
        bool defaultPool;
    };

    IDirect3DTexture9* makeDynamic(Dynamic& d) {
        IDirect3DTexture9* texture = nullptr;
        const auto format = static_cast<D3DFORMAT>(d.format);
        if (SUCCEEDED(m_device->CreateTexture(d.width, d.height, 1, D3DUSAGE_DYNAMIC, format, D3DPOOL_DEFAULT,
                                              &texture, nullptr))) {
            d.defaultPool = true;
            return texture;
        }
        if (SUCCEEDED(m_device->CreateTexture(d.width, d.height, 1, 0, format, D3DPOOL_MANAGED, &texture, nullptr))) {
            d.defaultPool = false;
            return texture;
        }
        return nullptr;
    }

    const Target* targetOf(TextureId id) const {
        for (const Target& t : m_targets) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }

    IDirect3DBaseTexture9* makeTarget(const Target& t) {
        if (t.floatColour) {
            IDirect3DTexture9* flat = nullptr;
            const bool made = SUCCEEDED(m_d3d->CheckDeviceFormat(m_adapterIndex, D3DDEVTYPE_HAL, m_pp.BackBufferFormat,
                                                                 D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R32F)) &&
                              SUCCEEDED(m_device->CreateTexture(t.width, t.height, 1, D3DUSAGE_RENDERTARGET,
                                                                D3DFMT_R32F, D3DPOOL_DEFAULT, &flat, nullptr));
            return made ? flat : nullptr;
        }
        for (D3DFORMAT format : {D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8}) {
            if (t.cube) {
                IDirect3DCubeTexture9* cube = nullptr;
                if (SUCCEEDED(m_device->CreateCubeTexture(t.width, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT,
                                                          &cube, nullptr))) {
                    return cube;
                }
            } else {
                IDirect3DTexture9* flat = nullptr;
                if (SUCCEEDED(m_device->CreateTexture(t.width, t.height, 1, D3DUSAGE_RENDERTARGET, format,
                                                      D3DPOOL_DEFAULT, &flat, nullptr))) {
                    return flat;
                }
            }
        }
        return nullptr;
    }

    IDirect3DSurface9* depthOf(uint32_t width, uint32_t height) {
        const uint64_t key = (static_cast<uint64_t>(width) << 32) | height;
        auto it = m_depths.find(key);
        if (it != m_depths.end()) return it->second;
        IDirect3DSurface9* depth = nullptr;
        m_device->CreateDepthStencilSurface(width, height, m_pp.AutoDepthStencilFormat, D3DMULTISAMPLE_NONE, 0, TRUE,
                                            &depth, nullptr);
        m_depths[key] = depth;
        return depth;
    }

    // The default pool's objects go before a reset and the targets come back after it (their contents lost).
    // The device's own references go first: the back buffer and its depth bound again, no texture on any sampler
    // (the pixel samplers 0 .. 15 and the vertex samplers D3DVERTEXTEXTURESAMPLER0 .. 3).
    void releaseTargets() {
        if (m_frameColor != nullptr) {
            // The picture's surface goes too: the back buffer bound, no depth (the reset binds its own).
            IDirect3DSurface9* back = nullptr;
            if (SUCCEEDED(m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) {
                m_device->SetRenderTarget(0, back);
                back->Release();
            }
            m_device->SetDepthStencilSurface(nullptr);
        } else if (m_backColor != nullptr) {
            m_device->SetRenderTarget(0, m_backColor);
            m_device->SetDepthStencilSurface(m_backDepth);
        }
        for (DWORD s = 0; s < 16; ++s) m_device->SetTexture(s, nullptr);
        for (DWORD s = D3DVERTEXTEXTURESAMPLER0; s <= D3DVERTEXTEXTURESAMPLER3; ++s) m_device->SetTexture(s, nullptr);
        release(m_backColor);
        release(m_backDepth);
        releaseFrame();
        for (auto& [key, depth] : m_depths) release(depth);
        m_depths.clear();
        releaseSamples();
        for (const Target& t : m_targets) release(m_textures[t.id - 1]);
        for (const Dynamic& d : m_dynamics) {
            if (d.defaultPool) release(m_textures[d.id - 1]);
        }
        for (IDirect3DQuery9*& q : m_queries) release(q);
        if (m_heatHaze != nullptr) m_heatHaze->releaseTargets();
        if (m_brightness != nullptr) m_brightness->ReleaseTargets();
        if (m_smaa != nullptr) m_smaa->ReleaseTargets();
        if (m_occlusion != nullptr) m_occlusion->ReleaseTargets();
        if (m_reflections != nullptr) m_reflections->ReleaseTargets();
        if (m_lampShadows != nullptr) m_lampShadows->ReleaseTargets();
        if (m_volumetric != nullptr) m_volumetric->ReleaseTargets();
        if (m_pointLights != nullptr) m_pointLights->ReleaseTargets();
        if (m_clouds != nullptr) m_clouds->ReleaseTargets();
        if (m_dilate != nullptr) m_dilate->ReleaseTargets();
        m_target = 0;
    }
    void remakeTargets() {
        for (const Target& t : m_targets) m_textures[t.id - 1] = makeTarget(t);
        for (Dynamic& d : m_dynamics) {
            if (d.defaultPool) m_textures[d.id - 1] = makeDynamic(d);
        }
        // The queries come back empty (their results asked before they are issued again are not ready).
        for (IDirect3DQuery9*& q : m_queries) m_device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &q);
    }

    // A frame not begun, logged the first time and every 300th after.
    void failed(const char* what, HRESULT hr) {
        if (m_failures++ % 300 == 0) {
            log::warning("d3d9: {} failed (0x{:08X}), {} frames not drawn", what, static_cast<uint32_t>(hr),
                         m_failures);
        }
    }

    // 0x005145F0, as the device is made or reset (0x0051359A): the colour, the depth and the stencil cleared, presented
    // and cleared again, so the frames that keep their colour start from black.
    void clearScreen() {
        m_device->Clear(0, nullptr, m_clearFlags | D3DCLEAR_TARGET, 0, 1.0f, 0);
        if (m_frameColor != nullptr) {
            // The fixed build's picture surface: the back buffer black as well.
            IDirect3DSurface9* back = nullptr;
            if (SUCCEEDED(m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) {
                m_device->ColorFill(back, nullptr, D3DCOLOR_XRGB(0, 0, 0));
                back->Release();
            }
        }
        m_device->Present(nullptr, nullptr, nullptr, nullptr);
        m_device->Clear(0, nullptr, m_clearFlags | D3DCLEAR_TARGET, 0, 1.0f, 0);
    }

    bool restore() {
        if (m_lost) {
            const HRESULT state = m_device->TestCooperativeLevel();
            if (state == D3DERR_DEVICELOST) {
                failed("TestCooperativeLevel", state);
                return false;
            }
            if (state == D3DERR_DEVICENOTRESET) {
                m_resetPending = true;
            }
            m_lost = false;
        }
        if (m_resetPending) {
            releaseTargets();
            m_profiler.release();
            const HRESULT hr = m_device->Reset(&m_pp);
            if (FAILED(hr)) {
                failed("Reset", hr);
                m_lost = true;
                return false;
            }
            m_resetPending = false;
            m_failures = 0;
            ++m_resets;
            remakeTargets();
            makeFrame();
            // A reset sets the device's states to their defaults; the callers keep the declaration they set last.
            setDeclaration(m_declaration);
            clearScreen();
            defaultSamplers();
            applyTextureQuality(false);
            m_viewportSet = false;
            m_scissor = false;
            log::info("d3d9: back buffer {}x{}", m_pp.BackBufferWidth, m_pp.BackBufferHeight);
        }
        return true;
    }

    IDirect3D9* m_d3d = nullptr;
    UINT m_adapterIndex = 0;  // DeviceDesc::adapter
    IDirect3DDevice9* m_device = nullptr;
    D3DPRESENT_PARAMETERS m_pp{};
    DWORD m_flags = 0;
    uint32_t m_vertexShaderVersion = 0;
    uint32_t m_pixelShaderVersion = 0;
    uint32_t m_sourceBlendCaps = 0;
    bool m_fullScreenGamma = false;
    uint32_t m_textureCaps = 0;
    AdapterInfo m_info;
    DWORD m_clearFlags = D3DCLEAR_ZBUFFER;  // [0x00719360]: the depth, and the stencil when the format has one
    std::string m_adapter;
    bool m_lost = false;
    std::thread::id m_thread = std::this_thread::get_id();  // the thread that made the device
    // Held by the frame (beginFrame .. endFrame) and by each call making, freeing, locking or writing a resource, so
    // that the loading thread's calls go between the frames (the device is not made multithreaded without -mtd3d).
    std::recursive_mutex m_deviceLock;
    std::atomic<int> m_waiting = 0;  // other threads waiting for the device
    std::atomic<uint64_t> m_services = 0;  // the device's thread's runs of the renderer's queue (frames, waits)
    bool m_inFrame = false;
    uint32_t m_failures = 0;
    uint32_t m_resets = 0;
    bool m_resetPending = false;
    std::vector<IDirect3DBaseTexture9*> m_textures;  // TextureId - 1
    std::vector<IUnknown*> m_shaders;            // ShaderId - 1: vertex and pixel shaders
    // A pixel shader's variants of the light cases (in kLightCases' order, 0: the shader itself) and the race's case.
    std::map<ShaderId, std::array<ShaderId, kLightCases.size()>> m_caseVariants;
    size_t m_case = lightCaseIndex(kBaseLightCase);
    std::vector<IDirect3DVertexDeclaration9*> m_declarations;  // DeclarationId - 1
    std::vector<IDirect3DResource9*> m_buffers;  // BufferId - 1: vertex and index buffers
    std::vector<IDirect3DQuery9*> m_queries;      // QueryId - 1
    std::vector<Target> m_targets;                // the render targets among the textures
    std::vector<Dynamic> m_dynamics;              // the textures the code writes
    std::map<uint64_t, IDirect3DSurface9*> m_depths;  // their depth buffers by size
    std::map<uint64_t, Samples> m_samples;            // the multisampled targets' samples by size
    TextureId m_samplesBound = 0;                     // the target whose samples are bound, not resolved yet
    IDirect3DSurface9* m_backColor = nullptr;     // the back buffer and its depth while a target is drawn into
    IDirect3DSurface9* m_backDepth = nullptr;
    TextureId m_target = 0;
    DeclarationId m_declaration = 0;
    // The fixed build's renderer options (the texture filter, the LOD bias and MSAA as the settings or the graphics
    // options gave them last, the adapter's anisotropy); the viewport and the scissor test the game set last (the
    // counter's clearing puts them back).
    // The game's samplers (its stages 0 .. 7); the shadow maps' from kShadowSampler on are the device's own.
    static constexpr DWORD kSamplers = 8;
    static constexpr DWORD kShadowSampler = 8;
    static constexpr std::array<SamplerState, 5> kQualityStates = {SamplerState::MinFilter, SamplerState::MagFilter,
                                                                   SamplerState::MipFilter, SamplerState::MaxAnisotropy,
                                                                   SamplerState::MipMapLodBias};
    static constexpr std::array<uint32_t, 5> kQualityDefaults = {D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, 1, 0};
    TextureQuality m_quality;
    uint32_t m_textureFilter = 0;
    uint32_t m_lodBias = 0;
    uint32_t m_maxAnisotropy = 0;
    uint32_t m_msaa = 0;
    std::array<std::array<uint32_t, 5>, kSamplers> m_gameSamplers = [] {
        std::array<std::array<uint32_t, 5>, kSamplers> states{};
        for (auto& s : states) s = kQualityDefaults;
        return states;
    }();
    bool m_showFps = false;
    FrameRateCounter m_frameRate;
    D3DVIEWPORT9 m_viewport{};
    bool m_viewportSet = false;
    bool m_scissor = false;
    // The fixed build's window without a frame: the picture's surface (its size, colour, depth, the multisampled
    // colour's resolved copy) and its place in the back buffer.
    uint32_t m_frameWidth = 0;
    uint32_t m_frameHeight = 0;
    D3DMULTISAMPLE_TYPE m_frameSamples = D3DMULTISAMPLE_NONE;
    DWORD m_frameQuality = 0;
    IDirect3DSurface9* m_frameColor = nullptr;
    IDirect3DSurface9* m_frameDepth = nullptr;
    IDirect3DSurface9* m_frameResolve = nullptr;
    bool m_stretch = false;
    RECT m_picture{};
    bool m_pictureInset = false;
    D3DTEXTUREFILTERTYPE m_pictureFilter = D3DTEXF_LINEAR;
    std::unique_ptr<d3d9::HeatHaze> m_heatHaze;        // the fixed build's heat haze, when on
    d3d9::Profiler m_profiler;                         // the debug builds' --profile
    IDirect3DSurface9* m_hazeSurface = nullptr;        // a scene target's surface under the heat haze's pass
    std::unique_ptr<BrightnessPass> m_brightness;      // the fixed build's brightness
    std::unique_ptr<SmaaPass> m_smaa;                  // the fixed build's SMAA, when on
    std::unique_ptr<AmbientOcclusionPass> m_occlusion;  // the fixed build's ambient occlusion, made at first use
    std::unique_ptr<LightReflectionPass> m_reflections;  // the fixed build's lights in the road, made at first use
    std::unique_ptr<LampShadowPass> m_lampShadows;       // the fixed build's cars' shadows from the street lamps
    std::unique_ptr<VolumetricLightPass> m_volumetric;   // the fixed build's light in the night's haze
    std::unique_ptr<CloudPass> m_clouds;                 // the fixed build's procedural clouds
    std::unique_ptr<DepthDilatePass> m_dilate;           // the sun's near cascade widened about its casters
    bool m_cloudsLogged = false;
    bool m_cloudViewLogged = false;
    std::unique_ptr<PointLightPass> m_pointLights;       // the fixed build's flames' light
    bool m_pointLightsLogged = false;
    bool m_volumetricLogged = false;
    bool m_lampShadowsLogged = false;
    bool m_reflectionsLogged = false;
    bool m_occlusionPossible = false;                   // not a pure device, shaders 3.0
    bool m_occlusionLogged = false;
    bool m_smaaPossible = false;                       // not a pure device, shaders 3.0
    bool m_smaaLogged = false;
    float m_brightnessLevel = 0.5f;
    bool m_brightnessLogged = false;
    ShaderReplacements m_replacements;                  // the fixed build's world light
    std::set<std::string> m_replaced;
};

}  // namespace

std::unique_ptr<IRenderDevice> createDevice(const DeviceDesc& desc, std::string* error) {
    auto device = std::make_unique<Device>();
    std::string message;
    if (!device->init(desc, &message)) {
        if (error != nullptr) {
            *error = message;
        }
        return nullptr;
    }
    return device;
}

}  // namespace oj::render::d3d9
