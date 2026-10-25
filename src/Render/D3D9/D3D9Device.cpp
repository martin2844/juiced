#include "Render/D3D9/D3D9Device.h"

#include "Core/Base/Log.h"

#include <windows.h>
#include <d3d9.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <format>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

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
        release(m_backColor);
        release(m_backDepth);
        for (auto& [key, depth] : m_depths) release(depth);
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
        D3DADAPTER_IDENTIFIER9 id{};
        if (SUCCEEDED(m_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id))) {
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

        D3DCAPS9 caps{};
        m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
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

        // 1.01 (0x00514EBB): the device asked for up to 250 times, 40 ms (Sleep) after both of a turn's tries failed;
        // the depth format D16 from the first failure on.
        HRESULT hr = D3DERR_NOTAVAILABLE;
        for (int tries = 0; tries < 250; ++tries) {
            hr = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, desc.window, flags, &m_pp, &m_device);
            if (FAILED(hr)) {
                m_pp.AutoDepthStencilFormat = D3DFMT_D16;
                hr = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, desc.window, flags, &m_pp, &m_device);
                if (FAILED(hr)) Sleep(40);
            }
            if (SUCCEEDED(hr)) break;
        }
        if (FAILED(hr)) {
            *error = std::format("IDirect3D9::CreateDevice failed (0x{:08X})", static_cast<uint32_t>(hr));
            return false;
        }
        m_clearFlags = D3DCLEAR_ZBUFFER | (hasStencil(m_pp.AutoDepthStencilFormat) ? D3DCLEAR_STENCIL : 0);
        clearScreen();
        return true;
    }

    Backend backend() const override { return Backend::D3D9; }
    std::string adapterName() const override { return m_adapter; }

    std::string description() const override {
        return std::format("{}x{} {} {} depth {}, multisample {}/{}, flags 0x{:X}", m_pp.BackBufferWidth,
                           m_pp.BackBufferHeight, m_pp.Windowed ? "windowed" : "full screen",
                           formatName(m_pp.BackBufferFormat), formatName(m_pp.AutoDepthStencilFormat),
                           static_cast<int>(m_pp.MultiSampleType), m_pp.MultiSampleQuality, m_flags);
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
        m_device->Clear(0, nullptr, m_clearFlags, 0xFF000000, 1.0f, 0);
        const HRESULT hr = m_device->BeginScene();
        if (FAILED(hr)) {
            failed("BeginScene", hr);
            m_deviceLock.unlock();
            return false;
        }
        m_inFrame = true;
        ++m_services;  // 0x00514C96
        return true;
    }

    void endFrame() override {
        m_device->EndScene();
        const HRESULT hr = m_device->Present(nullptr, nullptr, nullptr, nullptr);
        if (hr == D3DERR_DEVICELOST) {
            log::warning("d3d9: the device is lost");
            m_lost = true;
        }
        m_inFrame = false;
        m_deviceLock.unlock();
    }

    void letLoadingRun() override {
        if (!onDeviceThread()) return;
        ++m_services;  // 0x0045A38A
        if (!m_inFrame) return;
        m_deviceLock.unlock();
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

    uint32_t width() const override { return m_pp.BackBufferWidth; }
    uint32_t height() const override { return m_pp.BackBufferHeight; }
    uint32_t vertexShaderVersion() const override { return m_vertexShaderVersion; }
    uint32_t pixelShaderVersion() const override { return m_pixelShaderVersion; }
    AdapterInfo adapterInfo() const override { return m_info; }
    uint32_t backBufferFormat() const override { return static_cast<uint32_t>(m_pp.BackBufferFormat); }
    uint32_t sourceBlendCaps() const override { return m_sourceBlendCaps; }
    bool fullScreenGamma() const override { return m_fullScreenGamma; }
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

    void setRenderTarget(TextureId id, uint32_t face) override {
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
        return t != nullptr ? t->width : m_pp.BackBufferWidth;
    }
    uint32_t targetHeight(TextureId id) const override {
        const Target* t = id != 0 ? targetOf(id) : nullptr;
        return t != nullptr ? t->height : m_pp.BackBufferHeight;
    }

    uint32_t resets() const override { return m_resets; }

    bool readTarget(TextureId id, std::vector<uint8_t>& pixels, uint32_t* pitch) override {
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
        return SUCCEEDED(m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode)) ? mode.Format : 0;
    }
    uint32_t textureCaps() const override { return m_textureCaps; }
    bool textureFormatOk(uint32_t display, uint32_t format) const override {
        return SUCCEEDED(m_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, static_cast<D3DFORMAT>(display),
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
        if (rect == nullptr) {
            m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            return;
        }
        RECT r{};
        r.left = std::max(0L, static_cast<LONG>(rect->x0));
        r.top = std::max(0L, static_cast<LONG>(rect->y0));
        r.right = std::min(static_cast<LONG>(m_pp.BackBufferWidth), static_cast<LONG>(rect->x1));
        r.bottom = std::min(static_cast<LONG>(m_pp.BackBufferHeight), static_cast<LONG>(rect->y1));
        m_device->SetScissorRect(&r);
        m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    }

    bool capture(const std::wstring& file) override {
        const auto use = useDevice();
        IDirect3DSurface9* back = nullptr;
        if (FAILED(m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) {
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
        if (FAILED(m_device->CreateVertexShader(reinterpret_cast<const DWORD*>(code), &shader))) {
            return 0;
        }
        m_shaders.push_back(shader);
        return static_cast<ShaderId>(m_shaders.size());
    }

    ShaderId createPixelShader(const uint32_t* code) override {
        const auto use = useDevice();
        IDirect3DPixelShader9* shader = nullptr;
        if (FAILED(m_device->CreatePixelShader(reinterpret_cast<const DWORD*>(code), &shader))) {
            return 0;
        }
        m_shaders.push_back(shader);
        return static_cast<ShaderId>(m_shaders.size());
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
        m_device->SetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(type), value);
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
        m_device->SetPixelShader(static_cast<IDirect3DPixelShader9*>(shaderAt(shader)));
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
    }
    void draw(uint32_t primitive, uint32_t startVertex, uint32_t primitiveCount) override {
        m_device->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive), startVertex, primitiveCount);
    }
    void setViewport(const Viewport& v) override {
        const D3DVIEWPORT9 vp{v.x, v.y, v.width, v.height, v.minZ, v.maxZ};
        m_device->SetViewport(&vp);
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
        const bool haveMode = SUCCEEDED(m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode));
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
                if (FAILED(m_d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat, colour,
                                                  desc.windowed)) ||
                    FAILED(m_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat,
                                                    D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depths[d])) ||
                    FAILED(m_d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat, colour,
                                                         depths[d]))) {
                    continue;
                }
                m_pp.BackBufferFormat = colour;
                m_pp.AutoDepthStencilFormat = depths[d];
                DWORD colourLevels = 0;
                DWORD depthLevels = 0;
                if (desc.antialiasing != 0 &&
                    SUCCEEDED(m_d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, colour,
                                                                desc.windowed, D3DMULTISAMPLE_NONMASKABLE,
                                                                &colourLevels)) &&
                    SUCCEEDED(m_d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, depths[d],
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
    };

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
        if (m_backColor != nullptr) {
            m_device->SetRenderTarget(0, m_backColor);
            m_device->SetDepthStencilSurface(m_backDepth);
        }
        for (DWORD s = 0; s < 16; ++s) m_device->SetTexture(s, nullptr);
        for (DWORD s = D3DVERTEXTEXTURESAMPLER0; s <= D3DVERTEXTEXTURESAMPLER3; ++s) m_device->SetTexture(s, nullptr);
        release(m_backColor);
        release(m_backDepth);
        for (auto& [key, depth] : m_depths) release(depth);
        m_depths.clear();
        for (const Target& t : m_targets) release(m_textures[t.id - 1]);
        for (const Dynamic& d : m_dynamics) {
            if (d.defaultPool) release(m_textures[d.id - 1]);
        }
        for (IDirect3DQuery9*& q : m_queries) release(q);
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
            // A reset sets the device's states to their defaults; the callers keep the declaration they set last.
            setDeclaration(m_declaration);
            clearScreen();
            log::info("d3d9: back buffer {}x{}", m_pp.BackBufferWidth, m_pp.BackBufferHeight);
        }
        return true;
    }

    IDirect3D9* m_d3d = nullptr;
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
    std::vector<IDirect3DVertexDeclaration9*> m_declarations;  // DeclarationId - 1
    std::vector<IDirect3DResource9*> m_buffers;  // BufferId - 1: vertex and index buffers
    std::vector<IDirect3DQuery9*> m_queries;      // QueryId - 1
    std::vector<Target> m_targets;                // the render targets among the textures
    std::vector<Dynamic> m_dynamics;              // the textures the code writes
    std::map<uint64_t, IDirect3DSurface9*> m_depths;  // their depth buffers by size
    IDirect3DSurface9* m_backColor = nullptr;     // the back buffer and its depth while a target is drawn into
    IDirect3DSurface9* m_backDepth = nullptr;
    TextureId m_target = 0;
    DeclarationId m_declaration = 0;
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
