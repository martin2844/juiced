#pragma once
// The engine's interface to the graphics API. The game code draws through IRenderDevice only; the backend
// (Render/D3D9: Direct3D 9, the stock game's API) implements it.

#include "Render/RenderStates.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace oj::render {

enum class Backend { D3D9, Metal };

const char* backendName(Backend backend);

// What the stock renderer takes from the settings and the command line (docs/spec/01-startup.md, "Renderer").
struct DeviceDesc {
    HWND window = nullptr;           // the back buffer takes its client size
    bool windowed = true;
    uint32_t depthBits = 32;         // RezDepth: 16 | 32
    uint32_t antialiasing = 0;       // 0: none, else the multisample level asked for
    bool presentIntervalOne = false;  // -r
    bool pureDevice = true;          // cleared by -nonpured3d
    bool multithreaded = false;      // -mtd3d
    bool softwareVertexProcessing = false;  // -s, -debugfx
};

struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

// A vertex of the debug triangles: position in pixels of the back buffer (0,0 top left), colour 0xAARRGGBB.
struct DebugVertex {
    float x = 0.0f, y = 0.0f;
    uint32_t argb = 0xFFFFFFFF;
};

enum class PixelFormat { DXT1, DXT3, DXT5, A8R8G8B8, X8R8G8B8, R5G6B5 };

// Mip levels largest first, as in a DDS file (DXT in 4 x 4 blocks).
struct TextureData {
    PixelFormat format = PixelFormat::A8R8G8B8;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t levels = 1;
    const uint8_t* pixels = nullptr;
    size_t size = 0;
};

using TextureId = uint32_t;  // 0: none

// The sampler states a texture brings (its .ddt header).
struct SamplerParams {
    uint32_t minFilter = 2;
    uint32_t magFilter = 2;
    uint32_t mipFilter = 2;
    uint32_t addressU = 1;
    uint32_t addressV = 1;
    float lodBias = 0.0f;
};

enum class Blend { Alpha, Additive };

// A rectangle of the back buffer in pixels; drawing outside it is cut off.
struct ClipRect {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

// A vertex of a screen quad: position in pixels of the back buffer, texture coordinates, colour 0xAARRGGBB.
struct QuadVertex {
    float x = 0.0f, y = 0.0f;
    float u = 0.0f, v = 0.0f;
    uint32_t argb = 0xFFFFFFFF;
};

// The 3D pipeline as the stock engine drives it: Direct3D 9's states, shaders from byte code and constants; the
// state, type and enum values are Direct3D 9's.
using ShaderId = uint32_t;       // 0: none (the fixed-function stage)
using DeclarationId = uint32_t;  // 0: none
using QueryId = uint32_t;        // 0: none
using BufferId = uint32_t;       // 0: none

// A D3DVERTEXELEMENT9; a declaration ends with the record {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0}.
struct VertexElement {
    uint16_t stream = 0;
    uint16_t offset = 0;
    uint8_t type = 0;
    uint8_t method = 0;
    uint8_t usage = 0;
    uint8_t usageIndex = 0;
};

// D3DMATERIAL9: diffuse, ambient, specular, emissive (r, g, b, a each), power.
struct FixedMaterial {
    float values[17] = {};
};

// D3DLIGHT9.
struct FixedLight {
    uint32_t type = 3;  // D3DLIGHT_DIRECTIONAL
    float diffuse[4] = {};
    float specular[4] = {};
    float ambient[4] = {};
    float position[3] = {};
    float direction[3] = {};
    float range = 0.0f;
    float falloff = 0.0f;
    float attenuation0 = 0.0f;
    float attenuation1 = 0.0f;
    float attenuation2 = 0.0f;
    float theta = 0.0f;
    float phi = 0.0f;
};

struct Viewport {
    uint32_t x = 0, y = 0, width = 0, height = 0;
    float minZ = 0.0f, maxZ = 1.0f;
};

// D3DADAPTER_IDENTIFIER9's numbers (the CONDITION annotations of effects test them).
struct AdapterInfo {
    uint16_t driverProduct = 0, driverVersion = 0, driverSubVersion = 0, driverBuild = 0;
    uint32_t vendorId = 0, deviceId = 0, subSysId = 0, revision = 0;
};

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual Backend backend() const = 0;
    virtual std::string adapterName() const = 0;
    // A line on the chosen formats and flags, for the log.
    virtual std::string description() const = 0;

    // false: the device is lost or not restored yet; the frame is skipped (no endFrame).
    virtual bool beginFrame(const Color& clear) = 0;
    virtual void endFrame() = 0;

    // New size of the window's client area; the back buffer follows at the next beginFrame.
    virtual void resize(uint32_t width, uint32_t height) = 0;

    virtual void drawDebugTriangles(const DebugVertex* vertices, uint32_t count) = 0;

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    // D3DCAPS9 VertexShaderVersion / PixelShaderVersion (0xFFFE0300 / 0xFFFF0300 for 3.0).
    virtual uint32_t vertexShaderVersion() const = 0;
    virtual uint32_t pixelShaderVersion() const = 0;
    virtual AdapterInfo adapterInfo() const = 0;
    // The back buffer's D3DFORMAT.
    virtual uint32_t backBufferFormat() const = 0;
    // D3DCAPS9's SrcBlendCaps.
    virtual uint32_t sourceBlendCaps() const = 0;
    // D3DCAPS9's Caps2 has D3DCAPS2_FULLSCREENGAMMA.
    virtual bool fullScreenGamma() const = 0;
    // SetGammaRamp on the swap chain 0: the flags and the red, green and blue ramps of 256 values each.
    virtual void setGammaRamp(uint32_t flags, const uint16_t* red, const uint16_t* green, const uint16_t* blue) = 0;

    // 0 when the data is not usable (the levels that fit are taken).
    virtual TextureId createTexture(const TextureData& data) = 0;
    // A cube: width x width faces +X, -X, +Y, -Y, +Z, -Z one after another, each with all its levels; 0 when the data
    // is not usable.
    virtual TextureId createCubeTexture(const TextureData& data) = 0;
    virtual void destroyTexture(TextureId texture) = 0;
    // A texture drawn into (the stock code's 0x0051F2A0 for a flat one, 0x005C8F00 for a cube): 32-bit colour, one
    // level, a depth buffer of its size kept by the device; its contents are lost with the device. 0 when it cannot
    // be made.
    virtual TextureId createRenderTarget(uint32_t width, uint32_t height, bool cube) = 0;
    // The drawing goes into a render target's colour (a cube's face 0 .. 5: +X, -X, +Y, -Y, +Z, -Z) and its depth
    // buffer, or with 0 into the back buffer again; the viewport becomes the whole target.
    virtual void setRenderTarget(TextureId target, uint32_t face) = 0;
    // A render target's or the back buffer's (0) size.
    virtual uint32_t targetWidth(TextureId target) const = 0;
    virtual uint32_t targetHeight(TextureId target) const = 0;
    // The resets of the device so far: each loses what the render targets held.
    virtual uint32_t resets() const = 0;
    // A flat render target's pixels read back (32-bit, rows of `pitch` bytes); false when they cannot be read.
    virtual bool readTarget(TextureId target, std::vector<uint8_t>& pixels, uint32_t* pitch) = 0;

    // The display mode's D3DFORMAT (IDirect3D9::GetAdapterDisplayMode); 0 when it cannot be read.
    virtual uint32_t displayFormat() const = 0;
    // D3DCAPS9's TextureCaps.
    virtual uint32_t textureCaps() const = 0;
    // IDirect3D9::CheckDeviceFormat of a texture of no usage in `format` (a D3DFORMAT) on a display in `display`.
    virtual bool textureFormatOk(uint32_t display, uint32_t format) const = 0;
    // A texture of one level whose pixels the code writes, in a D3DFORMAT: dynamic in the default pool, else in the
    // managed one; 0 when neither can be made. The default pool's comes back after a reset with its pixels undefined.
    virtual TextureId createDynamicTexture(uint32_t width, uint32_t height, uint32_t format) = 0;
    // Its whole level locked (no flags): the first byte and the bytes from a row to the next; false when it cannot be.
    virtual bool lockTexture(TextureId texture, void** bits, int32_t* pitch) = 0;
    virtual void unlockTexture(TextureId texture) = 0;
    // DrawPrimitiveUP: primitives of a D3DPRIMITIVETYPE from vertices in memory, in the FVF set last.
    virtual void drawUp(uint32_t primitive, uint32_t primitiveCount, const void* vertices, uint32_t stride) = 0;

    // Corners top left, top right, bottom left, bottom right, in pixels of the back buffer as Direct3D 9 takes
    // transformed vertices (pixel centres at whole numbers); texture 0 draws the colours only. A texture is sampled
    // with linear filters and wrapping, or with the sampler states its caller set on stage 0 (samplerSet).
    virtual void drawQuad(const QuadVertex (&corners)[4], TextureId texture, Blend blend, bool samplerSet = false) = 0;
    // A line of one colour between two points in pixels of the back buffer (a line list of transformed vertices).
    virtual void drawLine(float x0, float y0, float x1, float y1, uint32_t argb) = 0;
    // nullptr: no clipping.
    virtual void setClip(const ClipRect* rect) = 0;

    // The back buffer (of the frame being drawn before endFrame; after it, what the swap left) written as a 24-bit
    // BMP, or a JPEG for a name ending in .jpg.
    virtual bool capture(const std::wstring& file) = 0;

    // 3D. Byte code ends with its end word 0x0000FFFF; 0 when the device refuses it.
    virtual ShaderId createVertexShader(const uint32_t* code) = 0;
    virtual ShaderId createPixelShader(const uint32_t* code) = 0;
    virtual DeclarationId createDeclaration(const VertexElement* elements) = 0;
    virtual BufferId createVertexBuffer(const void* data, uint32_t bytes) = 0;
    virtual BufferId createIndexBuffer(const uint16_t* indices, uint32_t count) = 0;
    virtual void destroyBuffer(BufferId buffer) = 0;
    // A vertex buffer's bytes from its start written anew.
    virtual void updateVertexBuffer(BufferId buffer, const void* data, uint32_t bytes) = 0;

    // Occlusion queries (D3DQUERYTYPE_OCCLUSION): whether the device has them, one made, its begin or end issued, and
    // its result: 0 with the pixels counted, 1 while not ready, -1 on an error. flush: D3DGETDATA_FLUSH.
    virtual bool occlusionQueries() const = 0;
    virtual QueryId createOcclusionQuery() = 0;
    virtual void issueQuery(QueryId query, bool begin) = 0;
    virtual int queryResult(QueryId query, bool flush, uint32_t* pixels) = 0;

    // The values: Render/RenderStates.h's (Cull, BlendFactor, ...) or the numbers the effects' files give.
    virtual void setRenderState(RenderState state, uint32_t value) = 0;
    virtual void setTextureStageState(uint32_t stage, TextureStageState type, uint32_t value) = 0;
    virtual void setSamplerState(uint32_t sampler, SamplerState type, uint32_t value) = 0;
    virtual void setTexture(uint32_t sampler, TextureId texture) = 0;
    virtual void setTransform(TransformState state, const float* matrix) = 0;
    virtual void setMaterial(const FixedMaterial& material) = 0;
    virtual void setLight(uint32_t index, const FixedLight& light) = 0;
    virtual void lightEnable(uint32_t index, bool enable) = 0;
    virtual void setFvf(uint32_t fvf) = 0;
    virtual void setNPatchMode(float segments) = 0;
    virtual void setVertexShader(ShaderId shader) = 0;
    virtual void setPixelShader(ShaderId shader) = 0;
    virtual void setVertexShaderConstantF(uint32_t start, const float* data, uint32_t count) = 0;
    virtual void setVertexShaderConstantI(uint32_t start, const int32_t* data, uint32_t count) = 0;
    virtual void setVertexShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) = 0;
    virtual void setPixelShaderConstantF(uint32_t start, const float* data, uint32_t count) = 0;
    virtual void setPixelShaderConstantI(uint32_t start, const int32_t* data, uint32_t count) = 0;
    virtual void setPixelShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) = 0;

    virtual void setDeclaration(DeclarationId declaration) = 0;
    virtual void setStream(BufferId buffer, uint32_t stride) = 0;
    virtual void setIndices(BufferId buffer) = 0;
    // Primitive; the vertices of the stream from 0, the indices from startIndex.
    virtual void drawIndexed(uint32_t primitive, uint32_t vertexCount, uint32_t startIndex,
                             uint32_t primitiveCount) = 0;
    // Primitive; the vertices of the stream from startVertex.
    virtual void draw(uint32_t primitive, uint32_t startVertex, uint32_t primitiveCount) = 0;
    virtual void setViewport(const Viewport& viewport) = 0;
    // ClearFlags.
    virtual void clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil) = 0;
};

// nullptr with *error set when the backend cannot start.
std::unique_ptr<IRenderDevice> createDevice(Backend backend, const DeviceDesc& desc, std::string* error);

}  // namespace oj::render
